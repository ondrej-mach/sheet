/**
 * Author: Ondrej Mach
 * VUT login: xmacho12
 * E-mail: ondrej.mach@seznam.cz
 */

/*
Implementation details
The program has no extra commands
It supports more selection commands in one run
Selections work with logical operator AND
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 10242
#define MAX_ROWS 200
#define MAX_CELL_LENGTH 101

#define MAX_DELIMITERS 101
#define DEFAULT_DELIMITERS " "

#define NUM_COMMANDS 25

#define DASH_NUMBER -1

// struct for table
// stores only one main delimiter
// others get replaced in function readTable
// the content of the data array is basically CSV
typedef struct {
    char data[MAX_LINE_LENGTH];
    char delimiter;
    bool rowSelected[MAX_ROWS+1]; // index 0 is not used
} table_t;

// always go together, easier to pass around
typedef struct {
    int argc;
    int index;
    char **argv;
} arguments_t;

// all program states
// these are returned by most functions that can fail in any way
typedef enum {
    SUCCESS = 0,
    NOT_FOUND,
    ERR_GENERIC,
    ERR_TOO_LONG,
    ERR_OUT_OF_RANGE,
    ERR_BAD_SYNTAX,
    ERR_TABLE_EMPTY,
    ERR_BAD_ORDER,
    ERR_BAD_TABLE
} state_t;

// categorizes every command
// program has to know which commands can be combined
typedef enum {
    NOT_SET = 0,
    DATA,
    LAYOUT,
    SELECTION,
} type_of_command_t;


// always go together, easier to pass around
typedef struct {
    char name[16];
    int numParameters;
    bool hasStringParameter;
    type_of_command_t type;
    // pointer for every function separately
    // what a perfect opportunity for union
    state_t (*fnZero)(table_t*);
    state_t (*fnOne)(table_t*, int);
    state_t (*fnTwo)(table_t*, int, int);
    // function with one integer argument and one string
    state_t (*fnOneStr)(table_t*, int, char*);
} command_t;

// checks, if the table is empty
bool isEmpty(table_t *table) {
    if (strcmp(table->data, "\n") <= 0)
        return true;

    return false;
}

// prints basic help on how to use the program
void printUsage() {
    const char *usageString = "\nUsage:\n"
        "./sheet [-d DELIM] [Commands for editing the table]\n"
        "or\n"
        "./sheet [-d DELIM] [Row selection] [Command for processing the data]\n";

    fprintf(stderr, "%s", usageString);
}

// prints error message according to the error state
void printErrorMessage(state_t err_state) {
    switch(err_state) {
        case NOT_FOUND:
            fputs("No commands found\n", stderr);
            printUsage();
            break;

        case ERR_GENERIC:
            fputs("Generic error\n", stderr);
            break;

        case ERR_TOO_LONG:
            fputs("Maximum file size is 10kiB\n", stderr);
            break;

        case ERR_OUT_OF_RANGE:
            fputs("Given cell coordinates are out of range\n", stderr);
            break;

        case ERR_BAD_SYNTAX:
            fputs("Bad syntax\n", stderr);
            break;

        case ERR_TABLE_EMPTY:
            fputs("Table cannot be empty\n", stderr);
            break;

        case ERR_BAD_ORDER:
            fputs("Commands are used in wrong order\n", stderr);
            printUsage();
            break;

        case ERR_BAD_TABLE:
            fputs("Table has different numbers of columns in each row\n", stderr);
            break;

        default:
            fputs("Unknown error\n", stderr);
            break;
    }
}

// shifts data in the array of table structure
// the table pointer is passed only to check for buffer overflow
state_t shiftData(char *p, int shift, table_t *table) {
    // doesnt need any shifting
    if (shift == 0)
        return SUCCESS;

    // we have to go from beginning not to overwrite our data
    if (shift < 0) {
        // condition is p[i-1], because \0 also has to be copied
        int i=0;
        while (p[i] != '\0') {
            p[i] = p[i-shift];
            i++;
        }
        p[i-shift] = '\0';

        return SUCCESS;
    }

    // we have to go from end
    if (shift > 0) {
        int lastIndex = strlen(p);

        // if we expand the data, we must check for buffer overflow
        if (&table->data[MAX_LINE_LENGTH] <= &p[lastIndex+shift]) {
            return ERR_TOO_LONG;
        }

        // then iterate through the array from the last index all the way to the first
        for (int i=lastIndex; i>=0; i--) {
            p[i+shift] = p[i];
        }
    }
    return SUCCESS;
}

// Takes in character and determines
// if it is end of cell
int endOfCell(char p, table_t *table) {
    if(p == table->delimiter)
        return 1;

    if(p == '\n')
        return 1;

    if(p == '\0')
        return 1;

    return 0;
}

// returns number of table's rows
int countRows(table_t *table) {
    int rows = 0;
    char c; // current character

    int i = 0;
    while ((c = table->data[i]) != '\0') {
        if (c == '\n') {
            rows++;
        }
        i++;
    }
    return rows;
}

// returns number of table's columns
int countColumns(table_t *table) {
    int columns = 1;

    char c; // current character

    int i=0;
    while ((c = table->data[i]) != '\n') {
        if (c == table->delimiter) {
            columns++;
        }
        i++;
    }
    return columns;
}

int isConsistent(table_t *table) {
    int numCols = -1; // not set, gets set after the first row and then stays constant
    int col = 1; // current column

    char c; // current character

    for (int i=0; (c = table->data[i]) != '\0'; i++) {

        if (c == table->delimiter) {
            col++;
        }

        if (c == '\n') {
            if (numCols == -1)
                numCols = col;

            if (col != numCols)
                return false;

            col = 1;
        }
    }
    return true;
}

// Takes argc and argv parameters from main
// Writes the delimiters into delimiters array
state_t readDelimiters(arguments_t *args, char delimiters[]) {
    strcpy(delimiters, DEFAULT_DELIMITERS);

    if (args->argc < 2) {
        return NOT_FOUND;
    }

    if (strcmp(args->argv[args->index], "-d") != 0) {
        return NOT_FOUND;
    }
    // All checks passed
    (args->index)++;
    strcpy(delimiters, args->argv[args->index]);
    (args->index)++;
    return SUCCESS;
}

// Reads table from stdin and saves it into the table structure
// The function also reads delimiters from arguments
// Returns program state
state_t readTable(arguments_t *args, table_t *table) {

    char delimiters[MAX_DELIMITERS];
    readDelimiters(args, delimiters);

    // set the table's main delimiter
    table->delimiter = delimiters[0];

    char c; // scanned character

    int i;
    for (i=0; scanf("%c", &c) != EOF; i++) {
        if(i+1 >= MAX_LINE_LENGTH) {
            return ERR_TOO_LONG;
        }

        // if scanned character is delimiter
        if (strchr(delimiters, c)) {
            // only main delimiter is stored in memory
            table->data[i] = delimiters[0];
            continue;
        }

        table->data[i] = c;
    }

    // fix faulty csv files
    // in memory there will be exactly one \n at the end
    table->data[i++] = '\n';
    // go back until there is exactly one \n left
    while (table->data[i-2] == '\n')
        i--;
    // write string termination charater
    table->data[i] = '\0';

    if (isConsistent(table))
        return SUCCESS;

    return ERR_BAD_TABLE;
}

// returns pointer to first character of the cell
// or NULL pointer, if coordinates are invalid
char *getCellPtr(int row, int column, table_t *table) {
    // check for bad coordinaters
    if ((row<1) || (column<1)) {
        return NULL;
    }

    int currentRow = 1;
    int currentColumn = 1;
    int currentCharacter = 0;

    // increment currentCharacter until both rowsrowsrowsrowsrowsrowsrows and colums are reached
    while ((currentRow<row) || (currentColumn<column)) {
        char c = table->data[currentCharacter];

        if (c == table->delimiter)
            currentColumn++;

        // end of line
        if (c == '\n') {
            // (n+1)st column is located there
            currentColumn++;
            if ((currentRow == row) && (currentColumn == column))
                break;

            // move onto the next row
            currentRow++;
            currentColumn = 1;
        }

        // end of table
        if (c == '\0')
            return NULL;

        currentCharacter++;
    }

    return &(table->data[currentCharacter]);
}


// reads string from table's cell
state_t readCell(table_t *table, int row, int column, char *content) {
    if (column<1 || column>countColumns(table))
        return ERR_OUT_OF_RANGE;

    char *cellPtr = getCellPtr(row, column, table);

    // should never happen, but just to make sure
    if (cellPtr == NULL)
        return ERR_GENERIC;

    int i=0;
    while (!endOfCell(cellPtr[i], table)) {
        content[i] = cellPtr[i];
        i++;
    }
    content[i] = '\0';

    return SUCCESS;
}

// writes passed string into chosen cell in table
state_t writeCell(table_t *table, int row, int column, char* content) {
    if (column<1 || column>countColumns(table))
        return ERR_OUT_OF_RANGE;

    char *cellPtr = getCellPtr(row, column, table);

    if (cellPtr == NULL)
        return ERR_GENERIC;

    // calculate length of both old and new cells
    int newCellLength = strlen(content);

    int oldCellLength=0;
    while (!endOfCell(cellPtr[oldCellLength], table))
        oldCellLength++;

    // how many characters to expand (can be negative)
    int shift = newCellLength - oldCellLength;

    state_t s = shiftData(cellPtr, shift, table);
    if (s != SUCCESS) {
        return s;
    }

    for (int i=0; i<newCellLength; i++) {
        cellPtr[i] = content[i];
    }

    return SUCCESS;
}

// inserts an empty row into the table
state_t irow(table_t *table, int row) {
    if (row < 1 || row > countRows(table)+1) {
        return ERR_OUT_OF_RANGE;
    }

    int numColumns = countColumns(table);

    char *p = getCellPtr(row, 1, table);

    state_t s = shiftData(p, numColumns, table);
    if (s != SUCCESS) {
        return s;
    }

    for (int i=0; i<numColumns-1; i++) {
        p[i] = table->delimiter;
    }
    p[numColumns-1] = '\n';

    return SUCCESS;
}

// appends an empty row to the table
state_t arow(table_t *table) {
    return irow(table, countRows(table)+1);
}

// deletes a row from the table
state_t drow(table_t *table, int row) {
    if (row < 1 || row > countRows(table))
        return ERR_OUT_OF_RANGE;

    char *p = getCellPtr(row, 1, table);

    // count how many characters have to be shifted out
    int i = 0;
    while (p[i++] != '\n');

    shiftData(p, -i, table);

    return SUCCESS;
}

// deletes multiple rows from the table
state_t drows(table_t *table, int m, int n) {
    if (n < m)
        return ERR_BAD_SYNTAX;

    for (int i=m; i<=n; i++) {
        state_t s = drow(table, m);
        if (s != SUCCESS)
            return s;
    }

    return SUCCESS;
}

// inserts an empty column into the table
state_t icol(table_t *table, int col) {
    state_t state;

    if (col < 1 || col > countColumns(table)+1)
        return ERR_OUT_OF_RANGE;

    int numRows = countRows(table);

    // for each row
    for (int i=1; i<=numRows; i++) {
        char *p = getCellPtr(i, col, table);
        state = shiftData(p, 1, table);
        *p = table->delimiter;
    }

    return state;
}

// appends an empty column to the table
state_t acol(table_t *table) {
    return icol(table, countColumns(table)+1);
}

// deletes a column from the table
state_t dcol(table_t *table, int col) {
    state_t state;

    if (col < 1 || col > countColumns(table))
        return ERR_OUT_OF_RANGE;

    if (countColumns(table) == 1)
        return ERR_TABLE_EMPTY;

    int numRows = countRows(table);

    // for each row
    for (int i=1; i<=numRows; i++) {
        char *p = getCellPtr(i, col, table);

        int j=0;
        while (!endOfCell(p[j], table))
            j++;
        // pointer will be at delimiter or \n

        // if is is the last column, delimiter in front of the column will be deleted
        if (p[j] == table->delimiter)
            state = shiftData(p, -(j+1), table);
        else
            state = shiftData(&p[-1], -(j+1), table);


        if (state != SUCCESS)
            return state;
    }
    return state;
}


// deletes multiple columns from the table
state_t dcols(table_t *table, int m, int n) {
    if (n < m)
        return ERR_BAD_SYNTAX;

    for (int i=m; i<=n; i++) {
        state_t s = dcol(table, m);
        if (s != SUCCESS)
            return s;
    }

    return SUCCESS;
}

// Prints the table into stdout
void printTable(table_t *table) {
    printf("%s", table->data);
}

// tries to read int from current argument
// if it succeeds returns true and increments argument index
// if there is -, the function assigns DASH_NUMBER constant
bool readInt(arguments_t *args, int *n) {
    if (args->index >= args->argc)
        return false;

    // special case for -
    if (strcmp(args->argv[args->index], "-") == 0) {
        *n = DASH_NUMBER;
        args->index++;
        return true;
    }

    char *pEnd;
    *n = strtol(args->argv[args->index], &pEnd, 10);
    if (*pEnd != '\0')
        return false;

    args->index++;
    return true;
}

// takes a float and rounds it
int roundNumber(float num) {
    if (num < 0)
        return num - 0.5;

    return num + 0.5;
}

// takes in string
// if there is a number inside,
// the function rounds it and writes it back to the string
void roundLine(char *buffer) {
    char *pEnd;
    double num = strtod(buffer, &pEnd);

    if (*pEnd == '\0') {
        // if the input value is valid
        sprintf(buffer, "%d", roundNumber(num));
    }
}

// takes in string
// if there is a number in it,
// the function makes integer out of the number and writes it back to the string
void intLine(char *buffer) {
    char *pEnd;
    double num = strtod(buffer, &pEnd);

    if (*pEnd == '\0') {
        // if the reading worked fine
        // and there's only number in the cell
        sprintf(buffer, "%d", (int)num);
    }
}

// takes in string and makes it uppercase
void upperLine(char *buffer) {
    int i=0;
    do {
        if ((buffer[i]>='a') && (buffer[i]<='z'))
            buffer[i] -= 'a'-'A';
    } while (buffer[i++] != '\0');
}

// takes in string and makes it lowercase
void lowerLine(char *buffer) {
    int i=0;
    do {
        if ((buffer[i]>='A') && (buffer[i]<='Z'))
            buffer[i] += 'a'-'A';
    } while (buffer[i++] != '\0');
}

// modifies each cell of the column, but only if it lies in the chosen row
// The string in cell is modified with the modFunction
state_t modifyData(table_t *table, int col, void(*modFunction)(char *)) {
    int numRows = countRows(table);
    for (int row=1; row<=numRows; row++) {
        if (table->rowSelected[row]) {
            char buffer[MAX_CELL_LENGTH];
            state_t state;

            state = readCell(table, row, col, buffer);
            if (state != SUCCESS)
                return state;

            modFunction(buffer);

            writeCell(table, row, col, buffer);
        }
    }
    return SUCCESS;
}

// all of these functions use modifyData()
state_t upperColumn(table_t *table, int col) {
    return modifyData(table, col, &upperLine);
}

state_t lowerColumn(table_t *table, int col) {
    return modifyData(table, col, &lowerLine);
}

state_t roundColumn(table_t *table, int col) {
    return modifyData(table, col, &roundLine);
}

state_t intColumn(table_t *table, int col) {
    return modifyData(table, col, &intLine);
}

// functions to rewrite data in columns
state_t setColumn(table_t *table, int col, char *content) {
    int numRows = countRows(table);

    for (int row=1; row<=numRows; row++) {
        if (table->rowSelected[row]) {
            state_t state = writeCell(table, row, col, content);
            if (state != SUCCESS)
                return state;
        }
    }
    return SUCCESS;
}

state_t copyColumn(table_t *table, int srcCol, int destCol) {
    int numRows = countRows(table);

    for (int row=1; row<=numRows; row++) {
        if (table->rowSelected[row]) {
            char buffer[MAX_CELL_LENGTH];
            state_t state = readCell(table, row, srcCol, buffer);
            if (state != SUCCESS)
                return state;

            state = writeCell(table, row, destCol, buffer);
            if (state != SUCCESS)
                return state;
        }
    }
    return SUCCESS;
}

state_t swapColumn(table_t *table, int col1, int col2) {
    int numRows = countRows(table);

    for (int row=1; row<=numRows; row++) {
        if (table->rowSelected[row]) {
            char content1[MAX_CELL_LENGTH];
            char content2[MAX_CELL_LENGTH];
            state_t state;

            state = readCell(table, row, col1, content1);
            if (state != SUCCESS)
                return state;

            state = readCell(table, row, col2, content2);
            if (state != SUCCESS)
                return state;

            writeCell(table, row, col1, content2);
            writeCell(table, row, col2, content1);
        }
    }
    return SUCCESS;
}

state_t moveColumn(table_t *table, int n, int m) {
    int pos = n;
    int endPos = m;
    if (n < m)
        endPos--;

    // not really efficient, lower level implementation would be faster
    while (pos != endPos) {
        // direction is -1 or +1
        int direction = 2*(pos < endPos) - 1;

        state_t state = swapColumn(table, pos, pos + direction);
        if (state != SUCCESS)
            return state;

        pos += direction;
    }
    return SUCCESS;
}


state_t selectRows(table_t *table, int start, int end) {
    int numRows = countRows(table);
    int numCols = countColumns(table);

    if (end == DASH_NUMBER) {
        // command like "rows 5 -" selectslines from 5 to the end
        if (start == DASH_NUMBER) {
            // special case for "rows - -", which selects only the last line
            start = numCols;
        }
        end = numCols;
    }

    if (start > end)
        return ERR_BAD_SYNTAX;

    if ((end > numCols) || (start < 1))
        return ERR_OUT_OF_RANGE;

    for (int row=1; row<=numRows; row++) {
        bool selected = (row >= start) && (row <= end);
        table->rowSelected[row] = table->rowSelected[row] && selected;
    }

    return SUCCESS;
}

state_t selectBeginsWith(table_t *table, int col, char *str) {
    int numRows = countRows(table);

    if (col < 1 || col > countColumns(table))
        return ERR_OUT_OF_RANGE;

    for (int row=1; row<=numRows; row++) {
        char content[MAX_CELL_LENGTH];
        readCell(table, row, col, content);
        bool selected = (strstr(content, str) == &content[0]);
        table->rowSelected[row] = table->rowSelected[row] && selected;
    }
    return SUCCESS;
}

state_t selectContains(table_t *table, int col, char *str) {
    int numRows = countRows(table);

    if (col < 1 || col > countColumns(table))
        return ERR_OUT_OF_RANGE;

    for (int row=1; row<=numRows; row++) {
        char content[MAX_CELL_LENGTH];
        readCell(table, row, col, content);
        // if str is found in the content of the cell
        bool selected = (strstr(content, str) != NULL);
        table->rowSelected[row] = table->rowSelected[row] && selected;
    }
    return SUCCESS;
}

// select all rows of the table
// different form all the selection functions
// assigns the value directly, whereas the other functions use and operator
void selectAll(table_t *table) {
    int numRows = countRows(table);

    for (int row=1; row<=numRows; row++)
        table->rowSelected[row] = true;
}

// reads command's parameters from args and executes it
state_t executeCommand(command_t *command, arguments_t *args, table_t *table) {
    // read all command's parameters
    int parameters[command->numParameters];

    for (int k=0; k < command->numParameters; k++) {
        if (!readInt(args, &parameters[k])) {
            return ERR_BAD_SYNTAX;
        }
    }

    if (!command->hasStringParameter) {
        switch (command->numParameters) {
            case 0: return command->fnZero(table);
            case 1: return command->fnOne(table, parameters[0]);
            case 2: return command->fnTwo(table, parameters[0], parameters[1]);
        }
    }

    // if command does have string parameter
    // there is only one type of command with string
    char strParameter[MAX_CELL_LENGTH];
    strcpy(strParameter, args->argv[args->index]);
    args->index++;
    return command->fnOneStr(table, parameters[0], strParameter);
}

// Can these two commands be after each other
bool isValidOrder (type_of_command_t currentCommand, type_of_command_t lastCommand) {
    switch (lastCommand) {
        case NOT_SET:
            return true;
        case LAYOUT:
            // there can be only layout command
            if (currentCommand == LAYOUT)
                return true;
            break;

        case DATA:
            // there can be only data command
            return false;
            break;

        case SELECTION:
            // there can be another selection or data command
            if ((currentCommand == DATA) || (currentCommand == SELECTION))
                return true;
            break;

        default: // should never happen
            break;
    }
    return false;
}

// takes in arguments and recognizes commands
// commands are executed right after they are found
state_t parseCommands(arguments_t *args, table_t *table) {
    command_t commands[NUM_COMMANDS] = {
        {.type=LAYOUT, .name="irow", .numParameters=1, .fnOne=irow},
        {.type=LAYOUT, .name="arow", .numParameters=0, .fnZero=arow},
        {.type=LAYOUT, .name="drow", .numParameters=1, .fnOne=drow},
        {.type=LAYOUT, .name="drows", .numParameters=2, .fnTwo=drows},
        {.type=LAYOUT, .name="icol", .numParameters=1, .fnOne=icol},
        {.type=LAYOUT, .name="acol", .numParameters=0, .fnZero=acol},
        {.type=LAYOUT, .name="dcol", .numParameters=1, .fnOne=dcol},
        {.type=LAYOUT, .name="dcols", .numParameters=2, .fnTwo=dcols},

        {.type=DATA, .name="cset", .numParameters=1, .hasStringParameter=true, .fnOneStr=setColumn},
        {.type=DATA, .name="tolower", .numParameters=1, .fnOne=lowerColumn},
        {.type=DATA, .name="toupper", .numParameters=1, .fnOne=upperColumn},
        {.type=DATA, .name="round", .numParameters=1, .fnOne=roundColumn},
        {.type=DATA, .name="int", .numParameters=1, .fnOne=intColumn},
        {.type=DATA, .name="copy", .numParameters=2, .fnTwo=copyColumn},
        {.type=DATA, .name="swap", .numParameters=2, .fnTwo=swapColumn},
        {.type=DATA, .name="move", .numParameters=2, .fnTwo=moveColumn},

        {.type=SELECTION, .name="rows", .numParameters=2, .fnTwo=selectRows},
        {.type=SELECTION, .name="beginswith", .numParameters=1, .hasStringParameter=true, .fnOneStr=selectBeginsWith},
        {.type=SELECTION, .name="contains", .numParameters=1, .hasStringParameter=true, .fnOneStr=selectContains}
    };

    if (args->index >= args->argc)
        return NOT_FOUND;

    type_of_command_t lastCommandType = NOT_SET;
    while (args->index < args->argc) {
        // if no command is found, it is bad syntax
        state_t state = ERR_BAD_SYNTAX;

        // go through all the commands and check if any name matches
        for (int i=0; i<NUM_COMMANDS; i++) {
            if (strcmp(commands[i].name, args->argv[args->index]) == 0) {
                args->index++; //successfully found a valid command
                if (!isValidOrder(commands[i].type, lastCommandType))
                    return ERR_BAD_ORDER;

                state = executeCommand(&commands[i], args, table);
                lastCommandType = commands[i].type;
                // we don't have to check this argument anymore
                break;
            }
        }
        if (state != SUCCESS)
            return state;
    }
    return SUCCESS;
}

int main(int argc, char **argv) {
    arguments_t args = {.argc=argc, .index=1, .argv=argv};

    table_t table;
    state_t state;

    state = readTable(&args, &table);
    // by default all rows are selected
    selectAll(&table);

    if (state == SUCCESS)
        state = parseCommands(&args, &table);

    if (isEmpty(&table))
        state = ERR_TABLE_EMPTY;

    if (state == SUCCESS) {
        printTable(&table);
        return EXIT_SUCCESS;
    }

    printErrorMessage(state);
    return EXIT_FAILURE;
}
