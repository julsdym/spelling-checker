#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>


#define MAX_WORD_LEN 256
#define BUFFER_SIZE 4096
#define INITIAL_DICT_SIZE 1000


typedef struct {
    char *original;  
    char *normalized;
} DictEntry;


typedef struct {
    DictEntry *entries;
    int count;
    int capacity;
} Dictionary;


Dictionary *create_dictionary() {
    Dictionary *dict = malloc(sizeof(Dictionary));
    dict->capacity = INITIAL_DICT_SIZE;
    dict->entries = malloc(dict->capacity * sizeof(DictEntry));
    dict->count = 0;
    return dict;
}


void normalize_word(const char *word, char *normalized) {
    int i = 0;
    while (word[i]) {
        if (word[i] != '\r' && word[i] != '\n')
            normalized[i] = tolower((unsigned char)word[i]);
        else
            normalized[i] = '\0';
        i++;
    }
    normalized[i] = '\0';
}


void add_word(Dictionary *dict, const char *word) {
    if (dict->count >= dict->capacity) {
        dict->capacity *= 2;
        dict->entries = realloc(dict->entries, dict->capacity * sizeof(DictEntry));
    }
    dict->entries[dict->count].original = strdup(word);
    dict->entries[dict->count].normalized = malloc(strlen(word) + 1);
    normalize_word(word, dict->entries[dict->count].normalized);
    dict->count++;
}


int compare_entries(const void *a, const void *b) {
    const DictEntry *ea = (const DictEntry *)a;
    const DictEntry *eb = (const DictEntry *)b;
    return strcmp(ea->normalized, eb->normalized);
}


void sort_dictionary(Dictionary *dict) {
    qsort(dict->entries, dict->count, sizeof(DictEntry), compare_entries);
}


void free_dictionary(Dictionary *dict) {
    for (int i = 0; i < dict->count; i++) {
        free(dict->entries[i].original);
        free(dict->entries[i].normalized);
    }
    free(dict->entries);
    free(dict);
}


Dictionary *load_dictionary(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open dictionary file '%s'\n", filename);
        return NULL;
    }


    Dictionary *dict = create_dictionary();
    char buffer[BUFFER_SIZE];
    char word[MAX_WORD_LEN];
    int word_len = 0;
    ssize_t bytes_read;


    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n' || c == '\r') {
                if (word_len > 0) {
                    word[word_len] = '\0';
                    add_word(dict, word);
                    word_len = 0;
                }
            } else if (word_len < MAX_WORD_LEN - 1) {
                word[word_len++] = c;
            }
        }
    }


    if (word_len > 0) {
        word[word_len] = '\0';
        add_word(dict, word);
    }


    close(fd);
    sort_dictionary(dict);
    return dict;
}
int is_valid_capitalization(const char *dict_word, const char *input_word) {
    size_t len = strlen(dict_word);  // Change int to size_t
    if (len != strlen(input_word)) return 0;
    int dict_has_lowercase = 0;
    int dict_has_uppercase = 0;
    for (size_t i = 0; i < len; i++) {  // Also change i to size_t
        if (islower((unsigned char)dict_word[i])) dict_has_lowercase = 1;
        if (isupper((unsigned char)dict_word[i])) dict_has_uppercase = 1;
    }
    int input_all_uppercase = 1;
    for (size_t i = 0; i < len; i++) {  // Change i to size_t here too
        if (isalpha((unsigned char)input_word[i]) && !isupper((unsigned char)input_word[i])) {
            input_all_uppercase = 0;
            break;
        }
    }
    if (dict_has_lowercase && dict_has_uppercase && input_all_uppercase) {
        return 0;
    }
   
    for (size_t i = 0; i < len; i++) {  // And here
        char d = dict_word[i];
        char inp = input_word[i];
        if (!isalpha((unsigned char)d)) {
            if (d != inp) return 0;
            continue;
        }
        if (tolower((unsigned char)d) != tolower((unsigned char)inp)) {
            return 0;
        }
        if (isupper((unsigned char)d) && !isupper((unsigned char)inp)) {
            return 0;
        }
    }
    return 1;
}


int word_in_dictionary(Dictionary *dict, const char *word) {
    char normalized[MAX_WORD_LEN];
    normalize_word(word, normalized);
    int left = 0, right = dict->count - 1;
    int found_idx = -1;
   
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(normalized, dict->entries[mid].normalized);
        if (cmp == 0) {
            found_idx = mid;
            break;
        }
        else if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
   
    if (found_idx == -1) {
        return 0;
    }
    int start_idx = found_idx;
    while (start_idx > 0 && strcmp(normalized, dict->entries[start_idx - 1].normalized) == 0) {
        start_idx--;
    }
    int idx = start_idx;
    while (idx < dict->count && strcmp(normalized, dict->entries[idx].normalized) == 0) {
        if (is_valid_capitalization(dict->entries[idx].original, word)) {
            return 1;
        }
        idx++;
    }
   
    return 0;
}


int is_all_digits_or_symbols(const char *word) {
    for (int i = 0; word[i]; i++) {
        if (isalpha((unsigned char)word[i])) return 0;
    }
    return 1;
}


const char *strip_leading_punctuation(const char *word) {
    while (*word && strchr("([{\'\"", *word)) word++;
    return word;
}


void strip_trailing_punctuation(char *word) {
    int len = strlen(word);
    while (len > 0 && !isalnum((unsigned char)word[len - 1])) len--;
    word[len] = '\0';
}


void check_word(Dictionary *dict, const char *word, const char *filename,
                int line, int col, int *error_found) {
    if (strlen(word) == 0) return;
    if (is_all_digits_or_symbols(word)) return;


    char processed[MAX_WORD_LEN];
    const char *stripped = strip_leading_punctuation(word);
    strcpy(processed, stripped);
    strip_trailing_punctuation(processed);


    if (strlen(processed) == 0 || is_all_digits_or_symbols(processed)) return;


    if (!word_in_dictionary(dict, processed)) {
        if (filename)
            printf("%s:%d:%d %s\n", filename, line, col, processed);
        else
            printf("%d:%d %s\n", line, col, processed);
        *error_found = 1;
    }
}


int check_file(Dictionary *dict, const char *filename, int show_filename) {
    int fd = (filename == NULL) ? STDIN_FILENO : open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 1;
    }


    char buffer[BUFFER_SIZE];
    char word[MAX_WORD_LEN];
    int word_len = 0, line = 1, col = 1, word_col = 1;
    ssize_t bytes_read;
    int error_found = 0;


    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];


            if (isspace((unsigned char)c)) {
                if (word_len > 0) {
                    word[word_len] = '\0';
                    check_word(dict, word, show_filename ? filename : NULL,
                               line, word_col, &error_found);
                    word_len = 0;
                }
                if (c == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            } else {
                if (word_len == 0) word_col = col;
                if (word_len < MAX_WORD_LEN - 1) word[word_len++] = c;
                col++;
            }
        }
    }


    if (word_len > 0) {
        word[word_len] = '\0';
        check_word(dict, word, show_filename ? filename : NULL,
                   line, word_col, &error_found);
    }


    if (filename != NULL) close(fd);
    return error_found;
}


int check_directory(Dictionary *dict, const char *path, const char *suffix, int *error_found) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", path);
        return 1;
    }


    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;


        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);


        struct stat st;
        if (stat(fullpath, &st) != 0) continue;


        if (S_ISDIR(st.st_mode)) {
            check_directory(dict, fullpath, suffix, error_found);
        } else if (S_ISREG(st.st_mode)) {
            int name_len = strlen(entry->d_name);
            int suffix_len = strlen(suffix);
            if (name_len >= suffix_len &&
                strcmp(entry->d_name + name_len - suffix_len, suffix) == 0) {
                if (check_file(dict, fullpath, 1))
                    *error_found = 1;
            }
        }
    }


    closedir(dir);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: spell [-s {suffix}] {dictionary} [{file or directory}]*\n");
        return EXIT_FAILURE;
    }


    const char *suffix = ".txt";
    int arg_idx = 1;


    if (strcmp(argv[arg_idx], "-s") == 0) {
        if (arg_idx + 1 >= argc) {
            fprintf(stderr, "Error: -s requires a suffix argument\n");
            return EXIT_FAILURE;
        }
        suffix = argv[arg_idx + 1];
        arg_idx += 2;
    }


    if (arg_idx >= argc) {
        fprintf(stderr, "Error: Dictionary file required\n");
        return EXIT_FAILURE;
    }


    const char *dict_file = argv[arg_idx++];
    Dictionary *dict = load_dictionary(dict_file);
    if (!dict) return EXIT_FAILURE;


    int error_found = 0;


    if (arg_idx >= argc) {
        if (check_file(dict, NULL, 0))
            error_found = 1;
    } else {
        int file_count = argc - arg_idx;
        for (int i = arg_idx; i < argc; i++) {
            struct stat st;
            if (stat(argv[i], &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    check_directory(dict, argv[i], suffix, &error_found);
                } else {
                    if (check_file(dict, argv[i], file_count > 1))
                        error_found = 1;
                }
            } else {
                fprintf(stderr, "Error: Cannot access '%s'\n", argv[i]);
                error_found = 1;
            }
        }
    }


    free_dictionary(dict);
    return error_found ? EXIT_FAILURE : EXIT_SUCCESS;
}


