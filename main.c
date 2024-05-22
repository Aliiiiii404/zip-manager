#include <getopt.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>
#include <unistd.h>

// we define the name of the directory where the files will be extracted to
#define EXTRACT_DIR "./extracted_files/"

// ascii art
void ascii_art() {
  printw("  #####  ######  #     #   #####   #####         #####\n");
  printw("  #      #    #  ##   ##   #   #   #    #        #    \n");
  printw("  #      #    #  # # # #   #####   #####  #####  #    \n");
  printw("  #      #    #  #  #  #   #       #   #         #    \n");
  printw("  #####  ######  #     #   #       #    #        #####\n");
  printw("\n");
}

// print choices
void print_choices(char *choices[], int max_choice, int choice) {
  for (int i = 0; i < max_choice; ++i) {
    if (choice - 1 == i)
      printw("[ * ]");
    else
      printw("[   ]");
    printw("%s\n", choices[i]);
  }
}

// returns the number of the files in the zip
int get_number_files(struct zip *zip_file) {
  int num_files = zip_get_num_entries(zip_file, 0);
  if (num_files < 0) {
    printw("Error getting number of files in zip file\n");
    return 1;
  }
  return num_files;
}

// get the files in the zip
int get_file_names(struct zip *zip_file, int num_files, char *file_names[]) {
  for (int i = 0; i < num_files; ++i) {
    const char *name = zip_get_name(zip_file, i, 0);
    if (!name) {
      printw("Error getting name of file %d\n", i);
    } else {
      // we cant do file_print_choices[i] = name;
      // so we allocate memory for the name of the file because it is a const
      size_t len = strlen(name);
      file_names[i] = malloc(len + 1);
      strcpy(file_names[i], name);
    }
  }
  return 0;
}

// prints the file names and asks the user wich one to print out
void get_file_content(struct zip *zip_file, int file_index) {
  file_index = file_index - 1;
  // Open the specified file
  struct zip_file *file = zip_fopen_index(zip_file, file_index, 0);
  const zip_error_t *error = zip_get_error(zip_file);
  if (!file) {
    // check if the file is encrypted
    if(zip_error_code_zip(error) == ZIP_ER_NOPASSWD)
      printw("This file is encrypted, try running -h for more infos\n");
    else
      printw("Error opening the file\n");
    return;
  }
  // Get the size of the file to print a message if empty
  struct zip_stat file_stat;
  zip_stat_index(zip_file, file_index, 0, &file_stat);
  printw("\n");
  if (file_stat.size == 0) {
    if(zip_error_code_zip(error) == ZIP_ER_NOPASSWD)
      printw("This file is encrypted, try running -h for more infos\n");
    else
      printw("This file is empty\n");
    return;
  } else {
    clear();
    // Print the content of the file
    char *buffer = malloc(file_stat.size);
    int len;
    while ((len = zip_fread(file, buffer, file_stat.size)) > 0) {
      fwrite(buffer, 1, len, stdout);
    }
    printw("\n");
  }
  // Close the file
  zip_fclose(file);
}

// extracting a file from the zip to a directory (./extracted_files/)
void extract_file(struct zip *zip_file, int file_index, char *file_name) {
  file_index = file_index - 1;
  // Open the specified file
  struct zip_file *file = zip_fopen_index(zip_file, file_index, 0);
  if (!file) {
    // check if the file is encrypted
    const zip_error_t *error = zip_get_error(zip_file);
    if(zip_error_code_zip(error) == ZIP_ER_NOPASSWD)
      printw("This file is encrypted, try running -h for more infos\n");
    else
      printw("Error opening the file\n");
    return;
  }
  // prepare the path to extract the file to
  char path[256];
  snprintf(path, sizeof(path), "%s/%s", EXTRACT_DIR, file_name);
  // create a new file with the same name in the new directory
  FILE *new_file = fopen(path, "wb");
  if (!new_file) {
    printw("Error creating file %s\n", path);
    endwin();
    return;
  }
  // add the content of the file to the new file
  char buffer[1024];
  int len;
  while ((len = zip_fread(file, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, len, new_file);
  }
  printw("\nFile %s extracted successfully to %s\n", file_name, EXTRACT_DIR);
  // Close the files
  fclose(new_file);
  zip_fclose(file);
  printw("\n");
}

// add a file to the current zip
void add_file(struct zip *zip_file, const char *zip_file_name){
  // get the file name from the user
  char full_file_name[256];
  int ch;
  int i = 0;
  printw("\npress ESC to go back to the menu\n");
  printw("Enter the path of the file : ");
  while(1){
    // make the while loop till the user press enter
    while ((ch = getch()) != '\n') {
      if (ch == KEY_BACKSPACE || ch == 127) {
        if (i > 0) {
          full_file_name[--i] = '\0';
          printw("\b \b");
          refresh();
        }
      // press ESC to go back to the menu
      }else if (ch == 27){
        return;
      }else {
        full_file_name[i++] = ch;
        full_file_name[i] = '\0';
        printw("%c", ch);
        refresh();
      }
    }
    // check if the file exists
    if (access(full_file_name, F_OK) != -1) {
      // creating a source for the file
      zip_source_t *source = zip_source_file(zip_file, full_file_name, 0, -1);
      if (!source) {
        printw("\nError creating source for file %s\n", full_file_name);
        return;
      }
      // removing the full path and keeping the name only
      char *file_name = strrchr(full_file_name, '/');
      if (file_name != NULL) {
        file_name++;
      } else {
        file_name = full_file_name;
      }
      // add the file to the zip
      int index = zip_file_add(zip_file, file_name, source, ZIP_FL_OVERWRITE);
      if (index < 0) {
        printw("\nError adding file %s to zip\n", file_name);
      } else {
        printw("\nFile %s added successfully to %s\n", file_name, zip_file_name);
        printw("\nPress any key to go back to the menu\n");
        refresh();
        getch();
      }
      break;
    } else {
      printw("\nFile does not exist, please enter a valid file path : ");
      i = 0;
    }
  }
}

// brute force the password of the encrypted file
void brute_force(){
  FILE *word_list;
  char zip_file_name[256];
  char password[50];
  int num_passwds_tested = 0;
  int ch;
  int i = 0;
  // get the file name from the user
  printw("\npress ESC to go back to the menu\n");
  printw("Enter the path of the file : ");
  while(1){
    // make the while loop till the user press enter
    while ((ch = getch()) != '\n') {
      if (ch == KEY_BACKSPACE || ch == 127) {
        if (i > 0) {
          zip_file_name[--i] = '\0';
          printw("\b \b");
          refresh();
        }
      // press ESC to go back to the menu
      }else if(ch == 27){
        return;
      }else {
        zip_file_name[i++] = ch;
        zip_file_name[i] = '\0';
        printw("%c", ch);
        refresh();
      }
    }
    // check if the file exists
    if (access(zip_file_name, F_OK) != -1) {
      // Open the zip file
      struct zip *zip = zip_open(zip_file_name, 0, NULL);
      if (zip) {
        // get the file number
        int num_files = get_number_files(zip);
        if (num_files < 0) {
          printw("This zip file is empty, try another one\n");
          printw("\nPress any key to go back to the menu\n");
          refresh();
          getch();
          return;
        }
        printw("\nTrying all the passwords in the word list, please wait !!\n");
        word_list = fopen("word-list", "r");
        if (word_list == NULL){
          printw("Error opening the file.\n");
          break;
        }
        int found_pass = 0;
        // test all the passwords in the word list
        while(fgets(password, sizeof(password), word_list)){
          password[strcspn(password, "\n")] = '\0';
          // chose the first file to test the passwords
          zip_file_t *zip_file = zip_fopen_encrypted(zip, "test1.txt", 0, password);
          if (zip_file) {
            found_pass = 1;
            zip_fclose(zip_file);
            break;
          }else{
            found_pass = 0;
          }
          num_passwds_tested++;
          // clear the password
          memset(password, 0, sizeof(password));
        }
        // close the file
        fclose(word_list);
        // close the zip file
        zip_close(zip);
        printw("\nNumber of passwords tested : %d\n", num_passwds_tested);
        if (found_pass == 0){
          printw("Password not found in the word list\n");
        }else if(found_pass == 1){
          printw("Password found : %s\n", password);
        }
        printw("\nPress any key to go back to the menu\n");
        refresh();
        getch();
        return;
      } else {
        printw("\nThis is not a zip file, enter a valid zip file : ");
        i = 0;
      }
    } else {
      printw("\nFile does not exist, please enter a valid file path : ");
      i = 0;
    }
  }
}

// print the help message
void print_help(const char *program_name) {
  printf("Usage : %s [OPTIONS]\n", program_name);
  printf("Options : \n");
  printf("    -f <filename> Specify the input file\n");
  /* printf("    -p <password> Specify the password for the encrypted file\n"); */
  printf("    -h Show this help\n");
}

// main function
int main(int argc, char *argv[]) {
  // getopt
  int opt;
  // zip file name, set to NULL by default and set by the user with -f
  const char *zip_file_name = NULL;
  // parse the options
  while ((opt = getopt(argc, argv, "f:h")) != -1) {
    switch (opt) {
    case 'f':
      zip_file_name = optarg;
      break;
    case 'h':
      print_help(argv[0]);
      endwin();
      exit(EXIT_SUCCESS);
    case '?':
      printf("Invalid option. Use -h or --help for usage.\n");
      endwin();
      exit(EXIT_FAILURE);
    }
  }
  // check if the filename is set
  if (zip_file_name == NULL) {
    printf("Invalid option. Use -h or --help for usage.\n");
    endwin();
    exit(EXIT_FAILURE);
  }
  // Open the zip file
  struct zip *zip_file_open = zip_open(zip_file_name, 0, NULL);
  if (!zip_file_open) {
    printf("Error opening zip file %s\n", zip_file_name);
    return 1;
  }
  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  //start color support 
  start_color();
  // Enable keypad for arrow keys and other special keys
  keypad(stdscr, TRUE);
  // this is for the main menu
  int choice = 1;
  char *choices[5] = {"Print the content of a file", "Extract a file","Add a file to the current zip","Bruteforce a zip file", "Quit"};
  int max_choice = 5;
  // the pressed key
  int key;
  // Main program loop
  while (1) {
    // Clear the screen
    clear();
    // Print the ascii art
    ascii_art();
    // Display the menu
    printw("Welcome to the zip viewer\n");
    printw("Choose an option:\n");
    print_choices(choices, max_choice, choice);
    refresh();
    // Get user input
    key = getch();
    // Handle arrow key presses
    if (key == KEY_UP) {
      choice = (choice == 1) ? max_choice : choice - 1;
    } else if (key == KEY_DOWN) {
      choice = (choice == max_choice) ? 1 : choice + 1;
    } else if (key == 10) { // Enter key pressed
      int file_choice = 1;
      int num_files = get_number_files(zip_file_open);
      char *file_names[num_files];
      // switch case for all the options
      switch (choice) {
      case 1:
        while (1) {
          clear();
          ascii_art();
          get_file_names(zip_file_open, num_files, file_names);
          printw("Choose a file to print\n");
          printw("Press ESC to go back to the menu\n");
          print_choices(file_names, num_files, file_choice);
          refresh();
          key = getch();
          if (key == KEY_UP) {
            file_choice = (file_choice == 1) ? num_files : file_choice - 1;
          } else if (key == KEY_DOWN) {
            file_choice = (file_choice == num_files) ? 1 : file_choice + 1;
          } else if (key == 10) {
            get_file_content(zip_file_open, file_choice);
            printw("\nPress any key to go back to the menu\n");
            refresh();
            getch();
            break;
          // presse Q to go back to the menu
          } else if (key == 27) {
            break;
          }
        }
        break;
      case 2:
        while (1) {
          clear();
          ascii_art();
          get_file_names(zip_file_open, num_files, file_names);
          printw("Choose a file to extract\n");
          printw("Press ESC to go back to the menu\n");
          print_choices(file_names, num_files, file_choice);
          refresh();
          key = getch();
          if (key == KEY_UP) {
            file_choice = (file_choice == 1) ? num_files : file_choice - 1;
          } else if (key == KEY_DOWN) {
            file_choice = (file_choice == num_files) ? 1 : file_choice + 1;
          } else if (key == 10) {
            extract_file(zip_file_open, file_choice, file_names[file_choice - 1]);
            printw("\nPress any key to go back to the menu\n");
            refresh();
            getch();
            break;
          } else if (key == 27) {
            break;
          }
        }
        break;
      case 3:
        clear();
        ascii_art();
        add_file(zip_file_open, zip_file_name);
        break;
      case 4:
        clear();
        ascii_art();
        brute_force();
        break;
      case 5:
        printw("Quitting...\n");
        zip_close(zip_file_open);
        endwin();
        return 0;
      default:
        printw("Invalid choice\n");
        refresh();
        getch();
        break;
      }
    }
  }
  return 0;
}
