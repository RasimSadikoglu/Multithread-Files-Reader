#include "../include/pargser.h"

#include <stdio.h>
#include <pthread.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
    pthread_mutex_t m_edit;
    char *word;
    char **files;
    int number_of_files;
} word_set;

word_set **unique_words;
int unique_words_size;
int unique_words_index;

pthread_mutex_t m_write_index = PTHREAD_MUTEX_INITIALIZER;

char** get_file_names(char*);

void* read_file(void*);

int main(int argc, char *argv[]) {

    int number_of_threads = -1;
    char *directory_name = NULL;

    pargser(argc, argv, "-d%*-n%d", &directory_name, &number_of_threads);

    if (number_of_threads == -1 || directory_name == NULL) {
        fprintf(stderr, "Error: Invalid arguments!\n"
                        "Usage: <exec> -d <directory_name> -n <number_of_threads>\n");
        exit(1);
    }
    
    char **file_names = get_file_names(directory_name);
    int number_of_files = 0;
    
    unique_words_size = 1 << 3;
    unique_words = malloc(sizeof(word_set*) * unique_words_size);
    printf("Main Thread: Allocated initial array of %d pointers.\n", unique_words_size);

    pthread_t thread_pool[number_of_threads];

    while (file_names[number_of_files] != NULL) {

        for (int i = 0; i < number_of_threads && file_names[number_of_files] != NULL; i++) {
            pthread_create(thread_pool + i, NULL, read_file, file_names[number_of_files]);
            printf("Main Thread: Assigned '%s' to worker thread %lu\n", file_names[number_of_files], (unsigned long)thread_pool[i]);
            number_of_files++;
        }

        int number_of_threads_working = number_of_files % number_of_threads == 0 ? number_of_threads : number_of_files % number_of_threads;

        for (int i = 0; i < number_of_threads_working; i++) {
            pthread_join(thread_pool[i], NULL);
        }

    }

    for (int i = 0; i < unique_words_index; i++) {
        free(unique_words[i]->files);
        free(unique_words[i]->word);
        free(unique_words[i]);
    }
    
    free(unique_words);

    for (int i = 0; file_names[i] != NULL; i++) free(file_names[i]);
    free(file_names);
}

char** get_file_names(char *directory_name) {
    char **file_names = calloc(sizeof(char*), 1);
    size_t index = 0;

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

        file_names[index] = malloc(file_name_size);
        sprintf(file_names[index], "%s/%s", directory_name, directory_entry->d_name);
        index++;

        file_names = realloc(file_names, sizeof(char*) * (index + 1));
        file_names[index] = NULL;
    }

    closedir(directory);

    return file_names;
}

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

int check_for_word(char *word) {
    
    for (int i = 0; i < unique_words_index; i++) {
        if (!strcmp(word, unique_words[i]->word)) return i;
    }

    return -1;
}

void* read_file(void *file_name) {

    char *file_content = read_file_content(file_name);

    char *buffer, *next = file_content;

    while ((buffer = strtok_r(next, " ", &next)) != NULL) {
        int index = check_for_word(buffer);

        if (index == -1) {

            word_set *unique_word = malloc(sizeof(word_set));

            pthread_mutex_init(&(unique_word->m_edit), NULL);
            
            unique_word->files = malloc(sizeof(char*));
            unique_word->files[0] = file_name;
            unique_word->number_of_files = 1;
            
            unique_word->word = malloc(strlen(buffer) + 1);
            strcpy(unique_word->word, buffer);

            pthread_mutex_lock(&m_write_index);

            if (unique_words_index == unique_words_size) {
                unique_words_size <<= 1;
                unique_words = realloc(unique_words, sizeof(word_set*) * unique_words_size);
                printf("Thread %lu: Re-allocated array of %d pointers.\n", (unsigned long)pthread_self(), unique_words_size);
            }

            int write_index = unique_words_index++;

            pthread_mutex_unlock(&m_write_index);

            unique_words[write_index] = unique_word;

            printf("Thread %lu: Added '%s' at index %d.\n", (unsigned long)pthread_self(), buffer, write_index);
        } else {
            printf("Thread %lu: The word '%s' has already located at index %d.\n", (unsigned long)pthread_self(), buffer, index);

            pthread_mutex_lock(&(unique_words[index]->m_edit));

            unique_words[index]->files = realloc(unique_words[index]->files, sizeof(char*) * (unique_words[index]->number_of_files + 1));
            unique_words[index]->files[unique_words[index]->number_of_files++] = file_name;

            pthread_mutex_unlock(&(unique_words[index]->m_edit));
        }

    }

    free(file_content);

    pthread_exit(NULL);
}