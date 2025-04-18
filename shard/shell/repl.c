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

static int repl_read_all(struct shard_source* self, struct shard_string* dest) {
    struct shell_repl* repl = (struct shell_repl*) self->userp;
    memcpy(dest, &repl->current_line, sizeof(struct shard_string));
    return 0;
}

static int repl_close(struct shard_source*) {
    return 0;
}

static void repl_buffer_dtor(struct shard_string*) {}

static void repl_init(struct shell_repl* repl, EditLine* el, History* history) {
    memset(repl, 0, sizeof(struct shell_repl));

    repl->current_line_no = 1;
    repl->el = el;
    repl->history = history;

    repl->source = (struct shard_source) {
        .line_offset = repl->current_line_no,
        .buffer_dtor = repl_buffer_dtor,
        .close = repl_close,
        .origin = "<repl>",
        .read_all = repl_read_all,
        .userp = repl
    };
}

static void repl_free(void) {
    history_end(shell.repl.history);
    el_end(shell.repl.el);
    shard_string_free(&shell.shard, &shell.repl.current_line);
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

    repl_init(&shell.repl, el, hist);
    atexit(repl_free);

    install_signal_handlers();

    const char* line;
    bool multiline = false;

    while(!shell.repl_should_close && (line = el_gets(el, &count)) != NULL) {
        if(!multiline) {
            shell.repl.source.line_offset = ++shell.repl.current_line_no;
            shell.repl.current_line.count = 0;
        }

        shard_string_appendn(&shell.shard, &shell.repl.current_line, line, count);

        if(shell.repl.current_line.count > 0 && shell.repl.current_line.items[shell.repl.current_line.count - 1] == '\\') {
            shell.repl.current_line.items[--shell.repl.current_line.count] = '\0';
            multiline = true;
            el_set(el, EL_PROMPT, multiline_prompt);
            continue;
        }

        multiline = false;
        el_set(el, EL_PROMPT, repl_prompt);

        err = shell_eval(&shell.repl.source, SH_EVAL_ECHO_RESULTS);
        if(err && shell.exit_on_error)
            break;
        
        char* save_line = shell.repl.current_line.items;
        trim_input(&save_line);
        if(strlen(save_line) > 0)
            history(hist, &hist_ev, H_ENTER, save_line);
    }
    
    return err;
}

