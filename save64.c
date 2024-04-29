#include "../include/save64.h"

// File handling functions
FileInfo* getFileInfo(FilePath savefile);
void* readSaveData(FilePath file_name, size_t file_size, Endian endian);

// Binary functions
void decodePlayerName(char* name, Region charset, FILE* fp);
uint16_t getChecksum16(uint8_t* buffer, uint16_t cs_offset, int width);
int countSetBits(uint8_t b);

// Print functions
void printHeader(const ootHeader* header);
void printWelcome(const char* title, const char* description);
void printHex(const char* label, int value);
void printBinary(uint16_t value, size_t width);
void printSave_oot(ootSave* savedata, FileInfo* file, int slot);
void printSave_maj(majSave* savedata);
void printSave_mario(marioSave* savedata);

int main(int argc, char* argv[])
{
    const FilePath program_name = argv[0];
    const char* description = "Nintendo 64 Save Viewer/Editor";
    uint8_t slot = 0;

    // Validate user input
    if (argc > 2 && argc < 4) {
        slot = (uint8_t)(atoi(argv[2]) - 1);
        if (slot > 2) {
            fprintf(stderr, "Invalid save slot number. It should be between 1 and 3.\n");
            return 1;
        }
    } else if (argc >= 4) {
        fprintf(stderr, "Too many arguments provided.\n");
        return 1;
    } else if (argc == 2) {
        slot = 0;
    } else {
        PlaySound(L"(D:/Programs/C/n64/zelda/gui/sound/OOT_Error.wav)", NULL, SND_FILENAME | SND_ASYNC );
        fprintf(stderr, "Usage: %s path/to/save/file -n\n", program_name);
        Sleep(561);
        return 1;
    }

    // Get basic input file info
    FileInfo* file = getFileInfo((FilePath)argv[1]);
    if (!file) {
       fprintf(stderr, "Error: Couldn't determine the game type.\n");
       return 1;
    }

   // // if (slot > 2 && file->game == Ocarina) {
   // //     fprintf(stderr, "Error: Ocarina of time does not contain a 4th save slot. Enter 1 or 2.\n");
   // //     return 1;
   // // }

    // Print program title banner
    enableAnsiEscapeCodes();
    printWelcome(description, file->title);

    ootHeader*  header     = NULL;           // pointer to Ocarina of Time header
    ootSave*    ootSav     = NULL;           // pointer to Ocarina of Time save data   
    majSave*    majSav     = NULL;           // pointer to Majora's Mask save data
    marioSave*  marioSav   = NULL;			 // pointer to Super Mario 64 save data
    uint16_t*   chkPtr     = NULL;           // pointer to checksum value in save data
    uint16_t    checksum   = 0x0000;         // byteswapped checksum from savedata
    uint16_t    actualChk  = 0x0000;         // the calculated checksum for comparison
    const char* sound   = L"D:/Programs/C/n64/zelda/sound/OOT_Secret.wav";

    switch (file->game) {

        case Ocarina: // reads the header first (32 bytes), and then blocks of save data (0x1450 bytes)
            header = (ootHeader*)readSaveData(file->path, SRA_HEADER_SIZE + (SRA_BLOCK_SIZE * 3), file->endian);
            ootSav = (ootSave*)((char*)header + sizeof(ootHeader));
            if (header->language != 0x0) {
                file->charset = PAL;
            }
            // print header and save file data
            printHeader(header);
            printSave_oot((ootSave*)((char*)ootSav + (slot * SRA_BLOCK_SIZE)), file, slot);

            // get the checksum from the save data, and then calculate the actual checksum
            chkPtr = (uint16_t*)((char*)&ootSav->checksum + (slot * SRA_BLOCK_SIZE));
            actualChk = getChecksum16((char*)ootSav + (slot * SRA_BLOCK_SIZE), file->chkOffset, 16);
            break;

        case Majora: // no header, just start reading blocks of save data (0x2000 bytes)
            majSav = (majSave*)readSaveData(file->path, (FLA_BLOCK_SIZE * 5), file->endian);

            // print save file data
            printSave_maj((majSave*)((char*)majSav + (slot * FLA_BLOCK_SIZE)));

            // get the checksum from the save data, and then calculate the actual checksum
            chkPtr = (uint16_t*)((char*)&majSav->checksum + (slot * FLA_BLOCK_SIZE));
            actualChk = getChecksum16((char*)majSav, file->chkOffset + (slot * FLA_BLOCK_SIZE), 8);
            break;

        case Mario: // no header, just start reading blocks of save data (0x2000 bytes)
			marioSav = (marioSave*)readSaveData(file->path, 0x200, file->endian);

			// print save file data
			printSave_mario((marioSave*)((char*)marioSav));

			// get the checksum from the save data, and then calculate the actual checksum
			chkPtr = (uint16_t*)((char*)&marioSav->checksum);
			actualChk = getChecksum16((char*)marioSav, file->chkOffset, 8);

            const char* mariosound = L"D:/Programs/C/n64/zelda/sound/sm64_coin.wav";
            sound = mariosound;
			break;
    }
    if (chkPtr != NULL)
         checksum = _byteswap_ushort(*chkPtr);

     // Print the actual checksum
     printf("Checksum ( %04x ?= %04x ):      ", checksum, actualChk);

    if ( checksum == actualChk ) {
        printf(ANSI_BG_GREEN ANSI_COLOR_WHITE " OK \t" ANSI_COLOR_RESET "\n"); 
    } 

    else {
        printf(ANSI_BG_RED ANSI_COLOR_WHITE "FAIL\t" ANSI_COLOR_RESET "\n");
        const char* errorsound = L"D:/Programs/C/n64/zelda/gui/sound/OOT_Error.wav";
        sound = errorsound;
    }

    PlaySound(sound, NULL, SND_FILENAME | SND_ASYNC );
    free(file);
    free(header);

    Sleep(1500);
}

FileInfo* 
getFileInfo(FilePath savefile)
{
	FileInfo* file = malloc(sizeof(FileInfo));

	// Initialize FileInfo path and file extension
	file->path = savefile;
	file->extension = strrchr((char*)savefile, '.') ? strrchr((char*)savefile, '.') : NULL;

	// Open the input file
    FILE* input = fopen(savefile, "rb");
    if (!input) {
        perror("Couldn't open the file");
        return 0;
    }

    // Container for magic identifying numbers
	uint32_t magic[3];

	// Read the first uint32_t at offset 0x24
    fseek(input, MAGIC_OFFSET_MM, SEEK_SET);
	fread(&magic[1], sizeof(uint32_t), 1, input);

	// Read the second uint32_t at offset 0x3C
	fseek(input, MAGIC_OFFSET_OOT - MAGIC_OFFSET_MM - sizeof(uint32_t), SEEK_CUR);
	fread(&magic[0], sizeof(uint32_t), 1, input);

	// Read the third uint32_t, 4 bytes before the end of the file
	fseek(input, -(char)sizeof(uint32_t), SEEK_END);
	fread(&magic[2], sizeof(uint32_t), 1, input);

	// Get file size
	fseek(input, 0, SEEK_END);
	file->size = ftell(input);

	int index = 0;		// index of magic number if found

	// 0x3C is DLEZ/ZELD, game is Ocarina of Time
	if (magic[0] == MAGIC_DLEZ || magic[0] == MAGIC_ZELD) {
		strncpy(file->title, "Ocarina of Time (1998)", sizeof(file->title) / sizeof(char));
		file->charset = NTSC;
		file->game = Ocarina;
		file->chkOffset = CHK_OFFSET_OOT;
		index = 0;
	}
	// 0x24 is DLEZ/ZELD, game is Majora's Mask
	else if (magic[1] == MAGIC_DLEZ || magic[1] == MAGIC_ZELD) {
		strncpy(file->title, "Majora's Mask (2000)", sizeof(file->title) / sizeof(char));
		file->game = Majora;
		file->charset = PAL;
		file->chkOffset = CHK_OFFSET_MM;
		index = 1;
	}
	// 0x24 is 0x09074948, game is Super Mario 64
	else if (magic[2] == MAGIC_MARIO) {
		strncpy(file->title, "Super Mario 64 (1996)", sizeof(file->title) / sizeof(char));
		file->game = Mario;
		file->charset = PAL;
		file->chkOffset = CHK_OFFSET_MARIO;
		index = 2;
	}	

	else {
		return NULL;
	}

	// ZELD = little endian | DLEZ = big endian | Mario 64 is always Big Endian
	if (magic[index] == MAGIC_ZELD) {
		file->endian = LittleE;
	}
	else if (magic[index] == MAGIC_DLEZ || magic[index] == MAGIC_MARIO) {
		file->endian = BigE;
	}

	fclose(input);
	return file;
}

void*
readSaveData(FilePath file_name, size_t file_size, Endian endian)
{
    // Open SRA file for reading
    FILE* input = fopen(file_name, "rb");
    if (!input) {
        perror("Couldn't read the file");
        return NULL;
    }

    // Allocate memory for header and saves
    uint8_t* buffer = malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(input);
        return NULL;
    }

	uint32_t dword;
	size_t bytesRead = 0;
	size_t bytesWritten = 0;
	size_t totalBytesRead = 0;

	while ((totalBytesRead < file_size) && ((bytesRead = fread(&dword, 1, sizeof(uint32_t), input)) == sizeof(uint32_t))) {
	    if (endian == LittleE) {
	        dword = _byteswap_ulong(dword);
	    }
		memcpy(buffer + totalBytesRead, &dword, sizeof(uint32_t));
	    totalBytesRead += sizeof(uint32_t);
	}

	if (totalBytesRead != file_size) {
		fprintf(stderr, "Couldn't read the save data.\n");
		free(buffer);
		fclose(input);
		return NULL;
	}

    fclose(input);
    return buffer;
}

void
decodePlayerName(char* name, Region charset, FILE* fp)
{
	unsigned char output;

	for (size_t i = 0; i < 8; ++i)
	{
		output = (unsigned char)name[i];

		switch (charset) {
			case NTSC:
				if (output >= 0xab && output <= 0xc4)        // 'A' - 'Z'
					output -= 0x6a;
				else if (output >= 0xc5 && output <= 0xde)   // 'a' - 'z'
					output -= 0x64;
				else if (output == 0xdf)   // Space ' '
					output = ' ';
				break;

			case PAL:
				if (output >= 0x0a && output <= 0x23)   	 // 'A' - 'Z'
					output += 0x37;
				else if (output >= 0x24 && output <= 0x3d)   // 'a' - 'z'
					output += 0x3d;
				else if (output == 0x3e)   // Space ' '
					output = ' ';
				break;
		}
		putc(output, fp);
	}
	putc('\n', fp);
}

uint16_t
getChecksum16(uint8_t* buffer, uint16_t cs_offset, int width)
{
	uint16_t end = cs_offset;
	uint16_t offset = 0x0000;
	uint16_t current = 0x0000;
	uint16_t checksum = 0x0000;

	while (offset < end) {
		if (width == 16) {
			current = (uint16_t)(*(buffer + offset) << 8 | *(buffer + offset + 1));
			offset += sizeof(uint16_t);
		}
		else if (width == 8) {
			current = (uint8_t)*(buffer + offset);
			offset += sizeof(uint8_t);
		}
		checksum += current;
	}

	return checksum;
}

void
printHeader(const ootHeader* header)
{
    printf("ID: ");  // Last 5 bytes contain string "ZELDA"
    for (int i = 0; i < 4; i++) {
        printf("0x%x ", (unsigned char)header->id[i]);
    }
    for (int i = 4; i < sizeof(header->id); i++) {
        putc(header->id[i], stdout);
    } putc('\n', stdout);

    printf( ANSI_BG_CYAN ANSI_COLOR_BLACK "       Options        " ANSI_COLOR_RESET "\n" );

    char sound[16] = "";
    switch (header->sound) {
    case 0x0:
        strncpy(sound, "Stereo", 8);
        break;
    case 0x1:
        strncpy(sound, "Mono", 8);
        break;
    case 0x2:
        strncpy(sound, "Headset", 8);
        break;
    case 0x3:
        strncpy(sound, "Surround", 16);
        break;
    default:
        strncpy(sound, "Unknown", 8);
        break;
    } printf("Sound: %s\n", sound);
   
    char ztarget[8] = "";
    switch (header->ztarget) {
    case 0x0:
        strcpy(ztarget, "Switch");
        break;
    case 0x1:
        strcpy(ztarget, "Hold");
        break;
    default:
        strcpy(ztarget, "Unknown");
        break;
    } printf("Z Target: %s\n", ztarget);

    char language[8] = "";
    switch (header->language) {
        case 0x0:
            strcpy(language, "English");
            break;
        case 0x1:
            strcpy(language, "German");
            break;
        case 0x2:
            strcpy(language, "French");
            break;
        default:
            strcpy(language, "Unknown");
            break;
    } printf("Language: %s\n", language);
}

void
printWelcome(const char* title, const char* description)
{
	printf( ANSI_BG_CYAN ANSI_COLOR_BLACK"****************************************************************"ANSI_COLOR_RESET "\n");
	printf( ANSI_BG_CYAN ANSI_COLOR_BLACK" %s  |  %-28s" ANSI_COLOR_RESET "\n", title, description);
	printf( ANSI_BG_CYAN ANSI_COLOR_BLACK"****************************************************************"ANSI_COLOR_RESET "\n");
}

void printHex(const char* label, int value)
{
	printf("%-31s   %02x\n", label, value);
}

void printBinary(uint16_t value, size_t width)
{
    int space = 0;
    for (int i = width; i > 0; i--, space++) {
        if (space == 4) {
            putc(' ', stdout);
        } space %= 4;
        putchar((value & (1 << i)) ? '1' : '0');
    }
    printf("\n");
}

// counts the number of '1' bits in an 8-bit unsigned integer
int countSetBits(uint8_t b)
{
	int count = 0;

	for(int i = 0; i < 7; i++) // ignore the MSB
		if(((b >> i) & 1) == 1)
			count++;
		
	return count;
}

void
printSave_oot(ootSave* savedata, FileInfo* file, int slot)
{
    // Byteswap variables
    uint32_t entranceIndex = _byteswap_ulong(savedata->entranceIndex);
    uint32_t ageModifier = _byteswap_ulong(savedata->ageModifier);
    uint16_t cutscene = _byteswap_ushort(savedata->cutscene);
    uint16_t worldTime = _byteswap_ushort(savedata->worldTime);
    uint32_t nightFlag = _byteswap_ulong(savedata->nightFlag);
    uint16_t deathCounter = _byteswap_ushort(savedata->deathCounter);
    uint16_t diskDriveOnly = _byteswap_ushort(savedata->diskDriveOnly);
    uint16_t heartContainers = _byteswap_ushort(savedata->heartContainers);
    uint16_t currentHealth = _byteswap_ushort(savedata->currentHealth);
    uint16_t rupees = _byteswap_ushort(savedata->rupees);
    uint16_t naviTimer = _byteswap_ushort(savedata->naviTimer);
    uint16_t currentlyEquippedEquipment = _byteswap_ushort(savedata->currentlyEquippedEquipment);
    uint16_t savedSceneIndex = _byteswap_ushort(savedata->savedSceneIndex);
    uint16_t obtainedEquipment = _byteswap_ushort(savedata->obtainedEquipment);
    uint32_t obtainedUpgrades = _byteswap_ulong(savedata->obtainedUpgrades);
    uint32_t questStatusItems = _byteswap_ulong(savedata->questStatusItems);
    uint16_t doubleDefenseHearts = _byteswap_ushort(savedata->doubleDefenseHearts);
    uint16_t goldSkulltulaTokens = _byteswap_ushort(savedata->goldSkulltulaTokens);
    uint32_t bigPoePoints = _byteswap_ulong(savedata->bigPoePoints);
    uint32_t checksum = _byteswap_ushort(savedata->checksum);

    // Print calls
    printf( "\n" ANSI_BG_CYAN ANSI_COLOR_BLACK "    File #%d    " ANSI_COLOR_RESET "\n", slot + 1);
    printf("Entrance Index:                 ");
    printLocationString(entranceIndex);
    printf("Age Modifier:                   %s\n", ageModifier ? "Child" : "Adult");
    printf("Cutscene:                       %04x\n", cutscene);
    printf("World Time:                     %04x\n", worldTime);
    printf("Night Flag:                     %s\n", nightFlag ? "Night" : "Day");
    printf("Magic String:                   %s\n", savedata->id);
    printf("Death Counter:                  %d\n", deathCounter);
    printf("Player Name:                    ");
    decodePlayerName(savedata->playerName, NTSC, stdout);
    printf("Disk Drive Only:                %04x\n", diskDriveOnly);
    printf("Current Health:                 %d/%d\n", currentHealth / 0x10, heartContainers / 0x10);
    printf("Magic Meter Size:               %x\n", savedata->magicMeterSize);
    printf("Current Magic:                  %.2f%%\n", ((double)savedata->currentMagic / (double)0x60) * 100.0);
    printf("Rupees:                         %03d\n", rupees);
    printf("Biggoron Sword Flag 1:          %s\n", savedata->biggoronSwordFlag1 ? "True" : "False");
    printf("Navi Timer:                     %04x\n", naviTimer);
    printf("Magic Flag 1:                   %s\n", savedata->magicFlag1 ? "True" : "False");
    printf("Magic Flag 2:                   %s\n", savedata->magicFlag2 ? "True" : "False");
    printf("Biggoron Sword Flag 2:          %x\n", savedata->biggoronSwordFlag2);
    printf("Saved Scene Index:              %04x\n", savedSceneIndex);

    // Skipping currentButtonEquips[7] (an array)

    printf("Currently Equipped Equipment:   %04x\n", currentlyEquippedEquipment);

    // Skipping inventory[24] (an array)

    printf("Item Amounts:\n");

    char* items[] = {
    "Deku Stick", "Deku Nuts", "Bombs", "Arrows",
    "?", "?", "Deku Seeds", "?", "Bombchus",
    "?", "?", "?", "?", "?", "?", "?", "?", "?"
    };
    
    for (int i = 0; i < 0xf; i++) {
        if (savedata->itemAmounts[i]) {
            printf(" *\t\t%-12s    %02d %s\n", "", savedata->itemAmounts[i], items[i]);
        }
    } putc('\n', stdout);

    printf("Magic Beans Bought:             %d\n", savedata->magicBeansBought);
    printf("Obtained Equipment:             ");
    printBinary(obtainedEquipment, 16);
    printf("Obtained Upgrades:              ");
    printBinary(obtainedUpgrades, 32);
    printf("Quest Status Items:             ");
    printBinary(questStatusItems, 32);

    // Skipping dungeonItems[0x14] (an array)

    // Skipping smallKeyAmounts[0x13] (an array)

    printf("Double Defense Hearts:          %d\n", doubleDefenseHearts / 0x100);
    printf("Gold Skulltula Tokens:          %d/100\n", goldSkulltulaTokens);
    printf("Big Poe Points:                 %d/10\n", bigPoePoints/100);

    // Skipping eventChkInf[14] (an array)

    // Skipping itemGetInf[4] (an array)

    // Skipping infTable[30] (an array)
}

void
printSave_maj(majSave* savedata)
{
	// Byteswap integer variables to little endian
	uint32_t entranceIndex = _byteswap_ulong(savedata->entranceIndex);
	uint32_t cutscene = _byteswap_ulong(savedata->cutscene);
	uint16_t worldTime = _byteswap_ushort(savedata->worldTime);
	uint16_t owlSaveLocation = _byteswap_ushort(savedata->owlSaveLocation);
	uint32_t nightFlag = _byteswap_ulong(savedata->nightFlag);
	uint32_t currentDay = _byteswap_ulong(savedata->currentDay);
	uint16_t heartContainers = _byteswap_ushort(savedata->heartContainers);
	uint16_t currentHealth = _byteswap_ushort(savedata->currentHealth);
	uint16_t rupees = _byteswap_ushort(savedata->rupees);
	uint16_t swordHealth = _byteswap_ushort(savedata->swordHealth);
	
	// Print save file data
	savedata->entranceIndex = _byteswap_ulong(savedata->entranceIndex);
	printf("Entrance Index:                 ");
	printLocationString(savedata->entranceIndex);
	printf("Equipped Mask:                  %02x\n", savedata->equippedMask);
	printf("Age Modifier:                   %s\n", savedata->ageModifier ? "Child" : "Adult");
	printf("Cutscene Number:                %08x\n", cutscene);
	printf("World Time:                     %04x\n", worldTime);
	printf("Owl Save Location:              %04x\n", owlSaveLocation);
	printf("Night Flag:                     %s\n", nightFlag ? "Night" : "Day");
	printf("Current Day:                    %x\n", currentDay);
	printf("Player Form:                    %02x\n", savedata->playerForm);
	printf("Have Tatl:                      %s\n", savedata->haveTatl ? "Yes" : "No");
	printf("Is Owl Save?:                   %s\n", savedata->isOwlSave ? "Yes" : "No");
	printf("Magic String:                   %s\n", savedata->id);
	printf("Player Name:                    ");
	decodePlayerName(savedata->playerName, PAL, stdout);
	printf("Current Health:                 %d/%d\n", currentHealth / 0x10, heartContainers / 0x10);
	printf("Magic Meter Size:               %d\n", savedata->magicMeterSize);
	printf("Current Magic:                  %.2f%%\n", ((double)savedata->currentMagic / (double)0x60) * 100.0);
	printf("Rupees:                         %03d\n", rupees);
	printf("Sword Health:                   %02x\n", swordHealth);
	printf("Double Defense Hearts:          %02x\n", savedata->doubleDefenseHearts);
}

// Function to print all values
void printSave_mario(marioSave* savedata)
{
	printf( "\n" ANSI_COLOR_BLACK ANSI_BG_CYAN " Cap Position Data " ANSI_COLOR_RESET "\n");
     //printf("Cap Position Data:\n");
	printf("capLevel  %26u\n", savedata->capLevel);
	printf("capArea   %26u\n", savedata->capArea);
	printf("capPos_x  %26d\n", savedata->capPos_x);
	printf("capPos_y  %26d\n", savedata->capPos_y);
	printf("capPos_z  %26d\n", savedata->capPos_z);

	 printf( "\n" ANSI_COLOR_BLACK ANSI_BG_CYAN " Castle Flags " ANSI_COLOR_RESET "\n");
     //printf("\nCastle Flags:\n");
    printf("%-27s", "castleStars: ");
	printBinary(savedata->castleStars, 8);
	printf("%-27s", "castleFlag: ");
	printBinary(savedata->castleFlag1, 8);
	printf("%-27s", "castleFlag2: ");
	printBinary(savedata->castleFlag2, 8);
	printf("%-27s", "castleFlag3: ");
	printBinary(savedata->castleFlag3, 8);

	// Levels
	printf( "\n" ANSI_COLOR_BLACK ANSI_BG_CYAN " Levels                  Coins  Stars " ANSI_COLOR_RESET "\n");
	printf("Bomb-omb Battlefield       %03d %3d/7\n", savedata->score1, countSetBits(savedata->stage1));
	printf("Whomp's Fortress           %03d %3d/7\n", savedata->score2, countSetBits(savedata->stage2));
	printf("Jolly Roger Bay            %03d %3d/7\n", savedata->score3, countSetBits(savedata->stage3));
	printf("Cool Cool Mountain         %03d %3d/7\n", savedata->score4, countSetBits(savedata->stage4));
	printf("Big Boo's Haunt            %03d %3d/7\n", savedata->score5, countSetBits(savedata->stage5));
	printf("Hazy Maze Cave             %03d %3d/7\n", savedata->score6, countSetBits(savedata->stage6));
	printf("Lethal Lava Land           %03d %3d/7\n", savedata->score7, countSetBits(savedata->stage7));
	printf("Shifting Sand Land         %03d %3d/7\n", savedata->score8, countSetBits(savedata->stage8));
	printf("Dire Dire Docks            %03d %3d/7\n", savedata->score9, countSetBits(savedata->stage9));
	printf("Snowman's Land             %03d %3d/7\n", savedata->score10, countSetBits(savedata->stage10));
	printf("Wet Dry World              %03d %3d/7\n", savedata->score11, countSetBits(savedata->stage11));
	printf("Tall Tall Mountain         %03d %3d/7\n", savedata->score12, countSetBits(savedata->stage12));
	printf("Tiny Huge Island           %03d %3d/7\n", savedata->score13, countSetBits(savedata->stage13));
	printf("Tick Tock Clock            %03d %3d/7\n", savedata->score14, countSetBits(savedata->stage14));
	printf("Rainbow Ride               %03d %3d/7\n", savedata->score15, countSetBits(savedata->stage15));
	
	printf( "\n" ANSI_COLOR_BLACK ANSI_BG_CYAN " Castle Secret Stars " ANSI_COLOR_RESET "\n");
	//printf("\nCastle Secret Stars:\n");
	printHex("bowser1RedCoins", savedata->bowser1RedCoins);
	printHex("bowser2RedCoins", savedata->bowser2RedCoins);
	printHex("bowser3RedCoins", savedata->bowser3RedCoins);
	printHex("princessSecretSlide", savedata->princessSecretSlide);
	printHex("metalCapRedCoins", savedata->metalCapRedCoins);
	printHex("wingCapRedCoins", savedata->wingCapRedCoins);
	printHex("vanishCapRedCoins", savedata->vanishCapRedCoins);
	printHex("marioWingsRedCoins", savedata->marioWingsRedCoins);
	printHex("princessSecretAquarium", savedata->princessSecretAquarium);
	printHex("unusedCakeScreen", savedata->unusedCakeScreen);

	printf( "\n" ANSI_COLOR_BLACK ANSI_BG_CYAN " Signature " ANSI_COLOR_RESET "\n");
    //printf("\nSignature:\n");
    printf("magicNumber: 0x%04X\n", _byteswap_ushort(savedata->magicNumber));
}

void modify_int(int arg)
{
    
}
