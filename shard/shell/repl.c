#include "resource.h"
#include "shell.h"
#include "eval.h"

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BSD libedit
#include <histedit.h>

#define DEFAULT_LINEBUFFER_SIZE 1024

static char* repl_prompt(EditLine* el) {
    (void) el;
    return shell.prompt;
}

static char* multiline_prompt(EditLine* el) {
    (void) el;
    return "> ";
}

static void trim_input(char** input) {
    while(isspace(**input)) (*input)++;
    size_t len = strlen(*input);
    if(len == 0)
        return;

    size_t last = len - 1;
    while(isspace((*input)[last])) (*input)[last--] = '\0';
}

/*
static unsigned char tab_completion(EditLine *el, int ch) {

}
*/

int shell_repl(void) {
    EditLine* el = el_init(shell.progname, stdin, stdout, stderr);
    if(!el)
        return errno;

    History* hist = history_init();
    if(!hist) {
        el_end(el);
        return errno;
    }

    HistEvent hist_ev;
    history(hist, &hist_ev, H_SETSIZE, shell.history_size);

    el_set(el, EL_PROMPT, repl_prompt);
    el_set(el, EL_HIST, history, hist);
    el_set(el, EL_SAFEREAD, 1);

    /*
    el_set(el, EL_ADDFN, "tab_completion", "Tab completion", tab_completion);
    el_set(el, EL_BIND, "\t", "tab_completion");
    */

    int err = 0;
    int count;
    const char* line;

    bool multiline = false;
    int line_buffer_size = DEFAULT_LINEBUFFER_SIZE;
    int line_buffer_count;
    char* line_buffer = malloc(line_buffer_size + 1);

    struct shell_resource resource;
    resource_from_string(line_buffer, 0, &resource);

    struct shell_state state;
    shell_state_init(&state);

    while(!shell.repl_should_close && (line = el_gets(el, &count)) != NULL) {
        resource_rewind(&resource);

        if(!multiline)
            line_buffer_count = 0; 

        if(count > line_buffer_size) {
            line_buffer_size = count;
            line_buffer = realloc(line_buffer, line_buffer_size + 1);
        }

        memcpy(line_buffer + line_buffer_count, line, count + 1);
        line_buffer_count += strlen(line);

        if(line_buffer_count > 1 && line_buffer[line_buffer_count - 2] == '\\') {
            line_buffer[line_buffer_count -= 2] = '\0';
            multiline = true;
            el_set(el, EL_PROMPT, multiline_prompt);
            continue;
        }

        multiline = false;
        el_set(el, EL_PROMPT, repl_prompt);

        err = shell_eval(&state, &resource);
        if(err && shell.exit_on_error)
            break;
        
        trim_input(&line_buffer);
        if((line_buffer_count = strlen(line_buffer)))
           history(hist, &hist_ev, H_ENTER, line_buffer);
    }
    
    shell_state_free(&state);
    resource_free(&resource);
    
    free(line_buffer);

    history_end(hist);
    el_end(el);
    
    return err;
}

