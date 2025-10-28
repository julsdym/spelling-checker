#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_WORD_LEN 256
#define BUFFER_SIZE 4096
#define INITIAL_DICT_SIZE 1000

typedef struct {
    char **words;
    int count;
    int capacity;
} Dictionary;

typedef struct {
    char *data;
    size_t size;
    size_t pos;
    int line;
    int col;
} FileBuffer;

// Dictionary functions
Dictionary *create_dictionary() {
    Dictionary *dict = malloc(sizeof(Dictionary));
    dict->capacity = INITIAL_DICT_SIZE;
    dict->words = malloc(dict->capacity * sizeof(char*));
    dict->count = 0;
    return dict;
}

void add_word(Dictionary *dict, const char *word) {
    if (dict->count >= dict->capacity) {
        dict->capacity *= 2;
        dict->words = realloc(dict->words, dict->capacity * sizeof(char*));
    }
    dict->words[dict->count++] = strdup(word);
}

int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

void sort_dictionary(Dictionary *dict) {
    qsort(dict->words, dict->count, sizeof(char*), compare_strings);
}

void free_dictionary(Dictionary *dict) {
    for (int i = 0; i < dict->count; i++) {
        free(dict->words[i]);
    }
    free(dict->words);
    free(dict);
}

// Load dictionary from file
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
            if (c == '\n') {
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

// Normalize word for searching (lowercase all letters)
void normalize_word(const char *word, char *normalized) {
    int i = 0;
    while (word[i]) {
        normalized[i] = tolower(word[i]);
        i++;
    }
    normalized[i] = '\0';
}

// Check if capitalization matches dictionary entry
int capitalization_matches(const char *dict_word, const char *input_word) {
    int i = 0;
    while (dict_word[i] && input_word[i]) {
        if (isupper(dict_word[i])) {
            if (dict_word[i] != input_word[i]) return 0;
        } else {
            if (tolower(input_word[i]) != dict_word[i]) return 0;
        }
        i++;
    }
    return dict_word[i] == input_word[i];
}

// Binary search in dictionary
int word_in_dictionary(Dictionary *dict, const char *word) {
    char normalized[MAX_WORD_LEN];
    normalize_word(word, normalized);

    int left = 0, right = dict->count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        char dict_normalized[MAX_WORD_LEN];
        normalize_word(dict->words[mid], dict_normalized);
        
        int cmp = strcmp(normalized, dict_normalized);
        
        if (cmp == 0) {
            // Check capitalization
            if (capitalization_matches(dict->words[mid], word)) {
                return 1;
            }
            // Check adjacent entries with same normalized form
            int i = mid - 1;
            while (i >= 0) {
                normalize_word(dict->words[i], dict_normalized);
                if (strcmp(normalized, dict_normalized) != 0) break;
                if (capitalization_matches(dict->words[i], word)) return 1;
                i--;
            }
            i = mid + 1;
            while (i < dict->count) {
                normalize_word(dict->words[i], dict_normalized);
                if (strcmp(normalized, dict_normalized) != 0) break;
                if (capitalization_matches(dict->words[i], word)) return 1;
                i++;
            }
            return 0;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return 0;
}

// Check if word is all digits/symbols
int is_all_digits_or_symbols(const char *word) {
    for (int i = 0; word[i]; i++) {
        if (isalpha(word[i])) return 0;
    }
    return 1;
}

// Strip leading opening punctuation
const char *strip_leading_punctuation(const char *word) {
    while (*word && (*word == '(' || *word == '[' || *word == '{' || 
                     *word == '\'' || *word == '"')) {
        word++;
    }
    return word;
}

// Strip trailing punctuation
void strip_trailing_punctuation(char *word) {
    int len = strlen(word);
    while (len > 0 && !isspace(word[len-1]) && !isalpha(word[len-1]) && !isdigit(word[len-1])) {
        len--;
    }
    word[len] = '\0';
}

// Process and check a word
void check_word(Dictionary *dict, const char *word, const char *filename, 
                int line, int col, int *error_found) {
    if (strlen(word) == 0) return;
    
    if (is_all_digits_or_symbols(word)) return;
    
    char processed[MAX_WORD_LEN];
    const char *stripped = strip_leading_punctuation(word);
    strcpy(processed, stripped);
    strip_trailing_punctuation(processed);
    
    if (strlen(processed) == 0) return;
    if (is_all_digits_or_symbols(processed)) return;
    
    if (!word_in_dictionary(dict, processed)) {
        if (filename) {
            printf("%s:%d:%d %s\n", filename, line, col, processed);
        } else {
            printf("%d:%d %s\n", line, col, processed);
        }
        *error_found = 1;
    }
}

// Check a file for spelling errors
int check_file(Dictionary *dict, const char *filename, int show_filename) {
    int fd;
    if (filename == NULL) {
        fd = STDIN_FILENO;
    } else {
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
            return 1;
        }
    }

    char buffer[BUFFER_SIZE];
    char word[MAX_WORD_LEN];
    int word_len = 0;
    int line = 1, col = 1;
    int word_col = 1;
    ssize_t bytes_read;
    int error_found = 0;

    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            
            if (isspace(c)) {
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
                if (word_len == 0) {
                    word_col = col;
                }
                if (word_len < MAX_WORD_LEN - 1) {
                    word[word_len++] = c;
                }
                col++;
            }
        }
    }

    if (word_len > 0) {
        word[word_len] = '\0';
        check_word(dict, word, show_filename ? filename : NULL, 
                  line, word_col, &error_found);
    }

    if (filename != NULL) {
        close(fd);
    }

    return error_found;
}

// Recursive directory traversal
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
                if (check_file(dict, fullpath, 1)) {
                    *error_found = 1;
                }
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

    // Parse -s option
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
    if (!dict) {
        return EXIT_FAILURE;
    }

    int error_found = 0;

    // If no files specified, read from stdin
    if (arg_idx >= argc) {
        if (check_file(dict, NULL, 0)) {
            error_found = 1;
        }
    } else {
        int file_count = argc - arg_idx;
        
        for (int i = arg_idx; i < argc; i++) {
            struct stat st;
            if (stat(argv[i], &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    check_directory(dict, argv[i], suffix, &error_found);
                } else {
                    if (check_file(dict, argv[i], file_count > 1)) {
                        error_found = 1;
                    }
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
