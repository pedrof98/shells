#include <sys/wait.h> // for waitpid() and associated macros
#include <unistd.h> // for fork()), chdir(), exec(), and pid_t
#include <stdlib.h> // for malloc(), free(), exit(), execvp(), realloc(), EXIT_FAILURE and EXIT_SUCCESS
#include <stdio.h> // for printf(), fprintf(), stderr, getchar() and perror()
#include <string.h> // for strcmp(), strtok()
#include <dirent.h>
#include <termios.h>

#define HISTORY_MAX 1000

// structure for history -- to store previous commands
typedef struct {
	char **commands; // array of command strings
	int count;		 // number of commands stored
	int capacity;    // max number of commands that can be stored
} History;

struct termios orig_termios;

// Function prototypes
History *history_init(void);
void history_add(History *hist, char *command);
void history_free(History *hist);
void history_save(History *hist);
void history_load(History *hist);
void lsh_loop(void);
char *lsh_read_line(void);
char **lsh_split_line(char *line);
int lsh_launch(char **args);
int lsh_execute(char **args);
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_num_builtins(void);
int lsh_ls(char **args);
int lsh_pwd(char **args);
int lsh_clear(char **args); 
int lsh_history(char **args);
char **get_completions(const char *partial);
void free_completions(char **completions);
int lsh_cat(char **args);
int lsh_grep(char **args);
int lsh_touch(char **args);
int lsh_echo(char **args);
int lsh_rm(char **args);

// Add global history
History *shell_history;

void enable_raw_mode() {
	tcgetattr(STDIN_FILENO, &orig_termios);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void lsh_loop(void)
{
	char *line;
	char **args;
	int status;

	do {
		printf("> ");
		line = lsh_read_line();
		args = lsh_split_line(line);
		status = lsh_execute(args);

		free(line);
		free(args);
	} while (status);
}


#define LSH_RL_BUFSIZE 1024
char *lsh_read_line(void)
{
	int bufsize = LSH_RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;
	int history_pos = shell_history->count;

	if (!buffer){
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
       	}

	enable_raw_mode();

	while (1) {
		// Read a character
		c = getchar();

		if (c ==27) { //ESC Sequence
			char seq[3];
			seq[0] = getchar();
			seq[1] = getchar();

			if (seq[0] == '[') {
					if (seq[1] == 'A') { //Up arrow
						if (history_pos > 0) {
							history_pos--;
							strcpy(buffer, shell_history->commands[history_pos]);
							printf("\r> %s\033[K", buffer); // clear to end of line
							position = strlen(buffer);
						}
						continue;
				}
				else if (seq[1] == 'B') { //Down arrow
						if (history_pos < shell_history->count -1) { // Fixed bounds check
							history_pos++;
							strcpy(buffer, shell_history->commands[history_pos]);
							printf("\r> %s\033[K]", buffer); // clear to end of line
							position = strlen(buffer);
						}
						else if (history_pos < shell_history->count -1) {
							// Clear line if at newest command
							buffer[0] = '\0';
							printf("\r> \033[K");
							position = 0;
						}
						continue;
				}
			}
			continue;
		}
		if (c == '\n') {
			buffer[position] = '\0';
			printf("\n");
			disable_raw_mode();
			if (position > 0) {
			history_add(shell_history, buffer);
			}
			return buffer;
		}

		if (c == '\t') { //tab key
			char **completions = get_completions(buffer);
			if (completions && completions[0]) {
				strcpy(buffer, completions[0]);
				printf("\r> %s", buffer);
				position = strlen(buffer);
			}
			free_completions(completions);
			continue;
		}

		if (c == 127) { //Backspace
			if (position > 0) {
				position--;
				printf("\b \b"); // move back, print space, move back again
			}
			continue;
		}

		if (c >= 32) {  // Printable characters
        	    buffer[position] = c;
           	 position++;
           	 printf("%c", c);
       		 }


		// If we have exceeded the buffer, reallocate
		if (position >= bufsize) {
			bufsize += LSH_RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "lsh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}




#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELM " \t\r\n\a" //Delimiters for splitting

char **lsh_split_line(char *line) // returns an array of strings 
{
	int bufsize = LSH_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char*)); // allocate array of string pointers
	char *token; 

	if (!tokens) {
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, LSH_TOK_DELM);  // Split on delimiters
	while (token != NULL) {
		tokens[position] = token;  // store pointer to token
		position++;

		// Resize if necessary
		if (position >= bufsize) {
			bufsize += LSH_TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if (!tokens) {
				fprintf(stderr, "lsh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		//Get next token
		token = strtok(NULL, LSH_TOK_DELM);
	}
	tokens[position] = NULL;
	return tokens;
}

/* C in the last function requires manual:
 * - memory allocation
 * - array resizing
 * - string splitting
 * - null termination
 * - memory management
*/



int lsh_launch(char **args)
{
	pid_t pid, wpid;
	int status;

	pid = fork(); // Creates a copy of current process
	if (pid == 0){
		// Child process
		// execvp replaces current process with new program
		// args[0] is program name
		// args is array of arguments
		if (execvp(args[0], args) == -1) {
			perror("lsh");
		}
		exit (EXIT_FAILURE);
	}
	else if (pid < 0) {
		// Error forking
		perror("lsh");
	}
	else {
		// Parent process
		do {
			// wait for child process to finish/change state
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	return 1;
}




// list of builtin commands followed by their corresponding functions

char *builtin_str[] = {
	"cd",
	"help",
	"exit",
	"ls",
	"pwd",
	"clear",
	"history",
	"cat",
	"grep",
	"touch",
	"echo",
	"rm"
};

int (*builtin_func[]) (char **) = {
	&lsh_cd,
	&lsh_help,
	&lsh_exit,
	&lsh_ls,
	&lsh_pwd,
	&lsh_clear,
	&lsh_history,
	&lsh_cat,
	&lsh_grep,
	&lsh_touch,
	&lsh_echo,
	&lsh_rm
};

int lsh_num_builtins() {
	return sizeof(builtin_str) / sizeof(char *);
}

// Builtin function implementations.

int lsh_cd(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
	}
	else {
		if (chdir(args[1]) != 0) {
			perror("lsh");
		}
	}
	return 1;
}


int lsh_help(char **args)
{
	int i;
	printf("Based off Stephen Brennan's LSH\n");
	printf("Type program names and arguments, and hit enter.\n");
	printf("The following are built in:\n");

	for (i=0; i < lsh_num_builtins(); i++) {
		printf(" %s\n", builtin_str[i]);
	}
	printf("Use the man command for information on other programs.\n");
	return 1;
}

int lsh_exit(char **args)
{
	return 0;
}


int lsh_ls(char **args)
{
	DIR *dir;
	struct dirent *entry;

	// open current directory if no argument
	const char *path = args[1] ? args[1] : ".";

	dir = opendir(path);
	if (dir == NULL) {
		perror("lsh");
		return 1;
	}

	while ((entry = readdir(dir)) != NULL) {
		// skip hidden files unless -a flag is present
		if (entry->d_name[0] == '.' && (!args[1] || strcmp(args[1], "-a") != 0))
			continue;
		printf("%s\n", entry->d_name);
	}
	closedir(dir);
	return 1;
}


int lsh_pwd(char **args)
{
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
	}
	else {
		perror("lsh");
	}
	return 1;
}


int lsh_clear(char **args) 
{
	//ANSI escape code to clear screen and move cursor to top
	printf("\033[2J\033[H");
	return 1;
}


int lsh_cat(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: expected argument to \"cat\"\n");
		return 1;
	}

	FILE *fp = fopen(args[1], "r");
	if (fp == NULL) {
		perror("lsh");
		return 1;
	}

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp)) {
		printf("%s", buffer);
	}

	fclose(fp);
	return 1;
}


int lsh_grep(char **args) {
	if (args[1] == NULL || args[2] == NULL) {
		fprintf(stderr, "lsh: grep requires pattern and filename\n");
		return 1;
	}

	FILE *fp = fopen(args[2], "r");
	if (fp == NULL) {
		perror("lsh");
		return 1;
	}

	char *pattern = args[1];
	char buffer[1024];
	int line_number = 1;

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (strstr(buffer, pattern)) {
			printf("%d: %s", line_number, buffer);
		}
		line_number++;
	}
	fclose(fp);
	return 1;
}


int lsh_touch(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: touch requires a filename\n");
		return 1;
	}

	FILE *fp = fopen(args[1], "a"); // 'a' creates file if doesn't exist
	if (fp == NULL) {
		perror("lsh");
		return 1;
	}
	fclose(fp);
	return 1;
}


int lsh_echo(char **args) {
	if (args[1] == NULL) {
		printf("\n");
		return 1;
	}

	int i = 1;
	while (args[i] != NULL && strcmp(args[i], ">") != 0) {
		printf("%s ", args[i]);
		i++;
	}

	if (args[i] && strcmp(args[i], ">") == 0 && args[i+1]) {
		// Redirect to file
		FILE *fp = fopen(args[i+1], "w");
		if (fp == NULL) {
			perror("lsh");
			return 1;
		}
		for (int j = 1; j < i; j++) {
			fprintf(fp, "%s ", args[j]);
		}
		fclose(fp);
	}
	else {
		printf("\n");
	}
	return 1;
}


int lsh_rm(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: rm requires a filename\n");
		return 1;
	}

	if (remove(args[1]) != 0) {
		perror("lsh");
		return 1;
	}
	return 1;
}


int lsh_execute(char **args)
{
	int i;

	if (args[0] == NULL) {
		// an empty command was entered
		return 1;
	}

	for (i=0; i < lsh_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args);
		}
	}
	return lsh_launch(args);
}


History *history_init(void) {
	History *hist = malloc(sizeof(History));
	hist->commands = malloc(sizeof(char*) * HISTORY_MAX);
	hist->count = 0;
	hist->capacity = HISTORY_MAX;
	return hist;
}

void history_add(History *hist, char *command) {
	if (hist->count >= hist->capacity) {
		// remove oldest command
		free(hist->commands[0]);
		for (int i = 0; i < hist->count - 1; i++) {
			hist->commands[i] = hist->commands[i + 1];
		}
		hist->count--;
	}
	hist->commands[hist->count++] = strdup(command);
}


void history_free(History *hist){
	for (int i = 0; i < hist->count; i++) {
		free(hist->commands[i]);
	}
	free(hist->commands);
	free(hist);
}


void history_save(History *hist) {
	FILE *fp = fopen(".shell_history", "w");
	if (!fp) {
		perror("lsh: history save");
		return;
	}
	for (int i = 0; i < hist->count; i++) {
		fprintf(fp, "%s\n", hist->commands[i]);
	}
	fclose(fp);
}

void history_load(History *hist) {
	FILE *fp = fopen(".shell_history", "r");
	if (!fp) return;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp)) {
		buffer[strcspn(buffer, "\n")] = 0;
		history_add(hist, buffer);
	}
	fclose(fp);
}


int lsh_history(char **args) {
	for (int i = 0; i < shell_history->count; i++) {
		printf("%d %s\n", i + 1, shell_history->commands[i]);
	}
	return 1;
}


//completion functions
char **get_completions(const char *partial) {
	char **completions = malloc(sizeof(char*) * LSH_TOK_BUFSIZE);
	int count = 0;


	//First let's try built-in commands
	for (int i = 0; i < lsh_num_builtins(); i++) {
		if (strncmp(partial, builtin_str[i], strlen(partial)) == 0) {
			completions[count++] = strdup(builtin_str[i]);
		}
	}


	// Now let's try with files
	DIR *dir = opendir(".");
	struct dirent *entry;

	while ((entry = readdir(dir)) && count < LSH_TOK_BUFSIZE) {
		if (strncmp(partial, entry->d_name, strlen(partial)) == 0) {
			completions[count++] = strdup(entry->d_name);
		}
	}
	closedir(dir);

	// show all matches if multiple found
	if (count > 1) {
		printf("\n");
		for (int i = 0; i < count; i++) {
			printf("%s ", completions[i]);
		}
		printf("\n> %s", partial);
	}

	completions[count] = NULL;
	return completions;
}

void free_completions(char **completions) {
	if (!completions) return;
	for (int i = 0; completions[i]; i++) {
		free(completions[i]);
	}
	free(completions);
}

int main(int argc, char **argv)
{
	shell_history = history_init();
	history_load(shell_history);
	// Load config files, if any
	//
	// Run command loop
	lsh_loop();

	history_save(shell_history);
	history_free(shell_history);

	// Perform any shutdown/cleanup
	
	return EXIT_SUCCESS;
}


