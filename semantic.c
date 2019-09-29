#include <string.h>

#include "ccatd.h"

void sema_func(Func *func) {
    // no duplicate parameter
    for (int i = 0; i < vec_len(func->params); i++) {
        Node *pi = vec_at(func->params, i);
        for (int j = i+1; j < vec_len(func->params); j++) {
            Node *pj = vec_at(func->params, j);
            if (!memcmp(pi->name, pj->name, pi->len)) {
                fnputs(stderr, func->name, func->len);
                fprintf(stderr, ": duplicate parameter `");
                fnputs(stderr, pi->name, pj->len);
                error("'");
            }
        }
    }
}
