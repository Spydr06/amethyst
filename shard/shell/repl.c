#include "shell.h"
#include "eval.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BSD libedit
#include <histedit.h>

struct repl {
    uintmax_t current_line_no;
    struct shard_string current_line;    

    struct shard_source source;
};

static int repl_read_all(struct shard_source* self, struct shard_string* dest) {
    struct repl* repl = (struct repl*) self->userp;
    memcpy(dest, &repl->current_line, sizeof(struct shard_string));
    return 0;
}

static int repl_close(struct shard_source*) {
    return 0;
}

static void repl_buffer_dtor(struct shard_string*) {}

static void repl_init(struct repl* repl) {
    memset(repl, 0, sizeof(struct repl));

    repl->current_line_no = 1;

    repl->source = (struct shard_source) {
        .line_offset = repl->current_line_no,
        .buffer_dtor = repl_buffer_dtor,
        .close = repl_close,
        .origin = "<repl>",
        .read_all = repl_read_all,
        .userp = repl
    };
}

static void repl_free(struct repl* repl) {
    shard_string_free(&shell.shard, &repl->current_line);
}

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

    struct repl repl;
    repl_init(&repl);

    const char* line;
    bool multiline = false;

    while(!shell.repl_should_close && (line = el_gets(el, &count)) != NULL) {
        if(!multiline) {
            repl.source.line_offset = ++repl.current_line_no;
            repl.current_line.count = 0;
        }

        shard_string_appendn(&shell.shard, &repl.current_line, line, count);

        if(repl.current_line.count > 0 && repl.current_line.items[repl.current_line.count - 1] == '\\') {
            repl.current_line.items[--repl.current_line.count] = '\0';
            multiline = true;
            el_set(el, EL_PROMPT, multiline_prompt);
            continue;
        }

        multiline = false;
        el_set(el, EL_PROMPT, repl_prompt);

        err = shell_eval(&repl.source, SH_EVAL_ECHO_RESULTS);
        if(err && shell.exit_on_error)
            break;
        
        char* save_line = repl.current_line.items;
        trim_input(&save_line);
        if(strlen(save_line) > 0)
            history(hist, &hist_ev, H_ENTER, save_line);
    }
    
    repl_free(&repl);

    history_end(hist);
    el_end(el);
    
    return err;
}

