#include <stdio.h>
#include <pthread.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// Error message and error macro
const char * const usage = "Error: Invalid arguments!\nUsage: <exec> -d <directory_name> -n <number_of_threads>\n";
#define CHECK_USAGE(CONDITION, MSG) if (!(CONDITION)) do { fprintf(stderr, (MSG)); exit(1); } while (0)

#define PARSING_TOKEN " \n\t"

// Every struct have their individual mutexes for update operation.
// Update operation adds file name to the files array.
typedef struct {
    pthread_mutex_t m_edit;
    char *word;
    char **files;
    int number_of_files;
} word_set;

// Unique words array and its mutex. Mutex is for getting correct index for new entries.
word_set **unique_words;
int unique_words_size;
int unique_words_index;
pthread_mutex_t m_write_index = PTHREAD_MUTEX_INITIALIZER;

// Main fills this file_names array with file names that are on the given directory
// Other threads take a file from this array and process it. If there is no file available anymore
// thread exits.
char **file_names;
int file_names_index;
pthread_mutex_t m_file_index = PTHREAD_MUTEX_INITIALIZER;

// Reader - Writer implementation for read and realloc.
// No reading happens while reallocing and vice versa.
pthread_mutex_t m_read = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_realloc = PTHREAD_MUTEX_INITIALIZER;
int read_count = 0;

void* read_file(void*);
void get_file_names(char *directory_name);

int main(int argc, char *argv[]) {

    // Take arguments and do the necessary error checking.
    int number_of_threads = -1;
    char *directory_name = NULL;

    CHECK_USAGE(argc == 5, usage);

    CHECK_USAGE((!strcmp(argv[1], "-d") || !strcmp(argv[3], "-d")), usage);
    CHECK_USAGE((!strcmp(argv[1], "-n") || !strcmp(argv[3], "-n")), usage);
    
    directory_name = strcmp(argv[1], "-d") ? argv[4] : argv[2];

    if (strcmp(argv[1], "-n")) number_of_threads = atoi(argv[4]);
    else number_of_threads = atoi(argv[2]);

    CHECK_USAGE(number_of_threads > 0, "There has to at least 1 thread!\n");

    // Fill file_names array with files on the given directory.
    get_file_names(directory_name);
    file_names_index = 0;

    // Inıtiliaze unique_words array for the first time.
    unique_words_size = 1 << 3;
    unique_words = malloc(sizeof(word_set*) * unique_words_size);
    printf("Main Thread: Allocated initial array of %d pointers.\n", unique_words_size);

    // Create thread pool
    pthread_t thread_pool[number_of_threads];

    // Start all threads.
    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(thread_pool + i, NULL, read_file, NULL);
    }

    // Wait for all threads
    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(thread_pool[i], NULL);
    }

    // Print summary
    printf("MAIN THREAD: All done (successfully read %d words with %d threads from %d files)\n", unique_words_index, number_of_threads, file_names_index);

    // Free memory before exit.
    for (int i = 0; i < unique_words_index; i++) {
        free(unique_words[i]->files);
        free(unique_words[i]->word);
        free(unique_words[i]);
    }
    
    free(unique_words);

    for (int i = 0; file_names[i] != NULL; i++) free(file_names[i]);
    free(file_names);
}

// Read directory using dirent library and save the file names.
void get_file_names(char *directory_name) {
    file_names = calloc(sizeof(char*), 1);
    file_names_index = 0;

    DIR *directory = opendir(directory_name);

    if (directory == NULL) {
        fprintf(stderr, "No such directory as '%s'!: ", directory_name);
        perror("");
        exit(errno);
    }

    struct dirent *directory_entry;
    while ((directory_entry = readdir(directory)) != NULL) {
        if (directory_entry->d_type != DT_REG) continue;
        if (strstr(directory_entry->d_name, ".txt") == NULL) continue;

        size_t file_name_size = strlen(directory_name) + strlen(directory_entry->d_name) + 2;

        file_names[file_names_index] = malloc(file_name_size);
        sprintf(file_names[file_names_index], "%s/%s", directory_name, directory_entry->d_name);
        file_names_index++;

        file_names = realloc(file_names, sizeof(char*) * (file_names_index + 1));
        file_names[file_names_index] = NULL;
    }

    closedir(directory);
}

// Read file and return its content as string.
char* read_file_content(char *file_name) {
    FILE *text_file = fopen(file_name, "r");

    if (text_file == NULL) pthread_exit(NULL);

    fseek(text_file, 0, SEEK_END);
    size_t file_size = ftell(text_file);
    rewind(text_file);

    char *file_content = malloc(file_size + 1);
    fread(file_content, 1, file_size, text_file);
    file_content[file_size] = '\0';

    fclose(text_file);

    return file_content;
}

void* read_file(void *arg) {

    // Get file name from file_names array.
    char *file_name;

    pthread_mutex_lock(&m_file_index);

    if (file_names[file_names_index] == NULL) { // If no more file available for to process, exit.
        pthread_mutex_unlock(&m_file_index);
        pthread_exit(NULL);
    }
    else file_name = file_names[file_names_index++];

    pthread_mutex_unlock(&m_file_index); // Release mutex lock
    
    printf("Main Thread: Assigned '%s' to worker thread %lu.\n", file_name, (unsigned long)pthread_self());

    // Get file content as string
    char *file_content = read_file_content(file_name);

    char *buffer, *next = file_content;

    while ((buffer = strtok_r(next, PARSING_TOKEN, &next)) != NULL) {
        int index = -1;

        // Reader - Writer implementation for read and realloc.
        // No reading happens while reallocing and vice versa.
        pthread_mutex_lock(&m_read);
        if (++read_count == 1) pthread_mutex_lock(&m_realloc);
        pthread_mutex_unlock(&m_read);

        for (int i = 0; i < unique_words_index; i++) {
            if (unique_words[i] == NULL) break;
            if (!strcmp(buffer, unique_words[i]->word)) {
                index = i;
                break;
            }
        }

        pthread_mutex_lock(&m_read);
        if (--read_count == 0) pthread_mutex_unlock(&m_realloc);
        pthread_mutex_unlock(&m_read);

        // Check for words index.
        if (index == -1) { // If word is not on the array, add.

            // Allocate necessary memory for the word and fill it.
            word_set *unique_word = malloc(sizeof(word_set));

            pthread_mutex_init(&(unique_word->m_edit), NULL);
            
            unique_word->files = malloc(sizeof(char*));
            unique_word->files[0] = file_name;
            unique_word->number_of_files = 1;
            
            unique_word->word = malloc(strlen(buffer) + 1);
            strcpy(unique_word->word, buffer);

            // Lock m_write_index mutex for getting the write index.
            pthread_mutex_lock(&m_write_index);

            if (unique_words_index == unique_words_size) { // If no space is available, reallocate more.
                
                pthread_mutex_lock(&m_realloc);
                unique_words_size <<= 1;
                unique_words = realloc(unique_words, sizeof(word_set*) * unique_words_size);
                memset(unique_words + unique_words_index, 0, sizeof(word_set*) * (unique_words_size - unique_words_index));
                pthread_mutex_unlock(&m_realloc);

                printf("Thread %lu: Re-allocated array of %d pointers.\n", (unsigned long)pthread_self(), unique_words_size);
            }

            int write_index = unique_words_index++;

            // After getting index and checking for available size release lock.
            pthread_mutex_unlock(&m_write_index);

            // Put word into the correct place.
            unique_words[write_index] = unique_word;

            printf("Thread %lu: Added '%s' at index %d.\n", (unsigned long)pthread_self(), buffer, write_index);
        } else { // If word is already on the unique_words array.
            printf("Thread %lu: The word '%s' has already located at index %d.\n", (unsigned long)pthread_self(), buffer, index);

            // Lock edit mutex.
            pthread_mutex_lock(&(unique_words[index]->m_edit));

            // Add file name to the files array.
            unique_words[index]->files = realloc(unique_words[index]->files, sizeof(char*) * (unique_words[index]->number_of_files + 1));
            unique_words[index]->files[unique_words[index]->number_of_files++] = file_name;

            // Release mutex lock.
            pthread_mutex_unlock(&(unique_words[index]->m_edit));
        }

    }

    // Free the file content
    free(file_content);

    // Go back to beginning check for new files.
    read_file(NULL);

    // Not necessary only for error suppresing.
    pthread_exit(NULL);
}