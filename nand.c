#include "nand.h"
#include "memory_tests.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

typedef struct output {
    struct output* next;
    nand_t* inputId; // do czego jest podlaczony
    size_t index; // do ktorego wyjscia jest podlaczony
}output;

struct nand {
    nand_t** inputs; // tablica wejsc dla bramek
    bool** boolInputs; // tablica wejsc dla sygnalow bool
    output* head; // poczatek listy wyjsc bramki
    output* tail; // koniec listy wyjsc bramki
    size_t numberOfInputs; // ilosc wejsc jaka posiada bramka
    size_t lastSeen; // ostatnio widziane prze funkcje evaluate
    size_t lastChanged; // ostatnie zmienienie wartosci/sciezki w evaluate
    size_t path; // dlugosc sciezki bramki
    size_t gatesTaken; // liczba aktualnie zajetych wejsc bramki
    bool value; // wartosc bramki
};

// usuwa korzen listy bramki ktora jest podlaczona do g_in
// odlaczamy to co jest podlaczone do k-tego wejscia g_in
void removeNode(nand_t* g_in, unsigned k) { 
    if (g_in->inputs[k] != NULL) {
        output* temp = g_in->inputs[k]->head;
        while (temp->next->inputId != g_in || temp->next->index != k)
            temp = temp->next;
        output* old = temp->next;
        temp->next = temp->next->next;
        free(old);
        g_in->gatesTaken--;
    }
}


nand_t* nand_new(unsigned n) {
    nand_t* newNand = (nand_t*)malloc(sizeof(nand_t));
    if (newNand == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    newNand->inputs = (nand_t**)malloc(sizeof(nand_t*) * n);
    newNand->boolInputs = (bool**)malloc(sizeof(bool*) * n);
    newNand->head = malloc(sizeof(output));
    newNand->tail = malloc(sizeof(output));
    if (newNand->tail == NULL || newNand->head == NULL ||
        newNand->boolInputs == NULL || newNand->inputs == NULL) {
        free(newNand->inputs);
        free(newNand->boolInputs);
        free(newNand->head);
        free(newNand->tail);
        free(newNand);
        errno = ENOMEM;
        return NULL;
    }
    for (size_t i = 0; i < n; ++i){
        newNand->inputs[i] = NULL;
        newNand->boolInputs[i] = NULL;
    }
    newNand->numberOfInputs = n;
    newNand->head->next = newNand->tail;
    newNand->tail->next = NULL;
    newNand->head->inputId = newNand->tail->inputId = NULL;
    newNand->head->index = newNand->tail->index = newNand->lastSeen = -1;
    newNand->path = newNand->gatesTaken = 0;
    newNand->value = false;
    newNand->lastChanged = -2;

    return newNand;
}

void nand_delete(nand_t* g) {
    if (g == NULL)
        return;
    // w bramkach do ktorych jest podlaczone g usuwa g z tablic wejsc
    output* temp = g->head->next;
    while (temp != g->tail) {
        temp->inputId->inputs[temp->index] = NULL;
        temp->inputId->gatesTaken--;
        temp = temp->next;
    }
    // w bramkach ktore sa podlaczone do g usuwa g z listy wyjsc
    for (size_t i = 0; i < g->numberOfInputs; i++)
        removeNode(g, i);

    //zwalnia pamiec zajmowana przez liste wyjsc g
    output* curr = g->head;
    output* currNext;
    while (curr != NULL) {
        currNext = curr->next;
        free(curr);
        curr = currNext;
    }
    free(g->inputs);
    free(g->boolInputs);
    free(g);
}

int nand_connect_nand(nand_t* g_out, nand_t* g_in, unsigned k) {
    if (g_in == NULL || g_out == NULL || k >= g_in->numberOfInputs) {
        errno = EINVAL;
        return -1;
    }
    output* newNode = (output*)malloc(sizeof(output));
    if (newNode == NULL) {
        errno = ENOMEM;
        return -1;
    }
    // usuwam poleczenie bramki jesli jest podlaczona do k-tego wejscia g_in
    removeNode(g_in, k);
    if (g_in->boolInputs[k] != NULL)
        g_in->gatesTaken--;
    // jesli do k-tego wejscia podlaczony jest sygnal bool to usuwam go
    g_in->boolInputs[k] = NULL;
    g_in->inputs[k] = g_out;

    newNode->inputId = g_in;
    newNode->index = k;
    newNode->next = g_out->head->next;
    g_out->head->next = newNode;
    g_in->gatesTaken++;
    return 0;
}

int nand_connect_signal(bool const* s, nand_t* g_in, unsigned k) {
    if (s == NULL || g_in == NULL || k >= g_in->numberOfInputs) {
        errno = EINVAL;
        return -1;
    }
    // usuwam poleczenie bramki jesli jest podlaczona do k-tego wejscia g_in
    removeNode(g_in, k);
    if (g_in->boolInputs[k] != NULL)
        g_in->gatesTaken--;

    g_in->inputs[k] = NULL;
    g_in->boolInputs[k] = (bool*)s;
    g_in->gatesTaken++;

    return 0;
}

ssize_t nand_fan_out(nand_t const* g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }

    output* temp = g->head->next;
    ssize_t count = 0;
    while (temp != g->tail) {
        temp = temp->next;
        count++;
    }

    return count;
}

void* nand_input(nand_t const* g, unsigned k) {
    if (g == NULL || k >= g->numberOfInputs) {
        errno = EINVAL;
        return NULL;
    }
    if (g->inputs[k] == NULL && g->boolInputs[k] == NULL) {
        errno = 0;
        return NULL;
    }

    return g->inputs[k] != NULL ? g->inputs[k] : (void*)g->boolInputs[k];
}

nand_t* nand_output(nand_t const* g, ssize_t k) {
    if (g == NULL || k < 0)
        return NULL;

    output* temp = g->head->next;
    for (ssize_t i = 0; i < k; i++) {
        temp = temp->next;
        if (temp == g->tail)
            return NULL;
    }

    return temp->inputId;
}

void evaluate(bool* err, nand_t* g, size_t callCount) {
    if (*err == true || g->lastChanged == callCount) 
        return;
    if (g->numberOfInputs != g->gatesTaken || 
        (g->lastSeen == callCount && g->lastChanged != callCount)) {
        *err = true;
        errno = ECANCELED;
        return;
    }
    g->lastSeen = callCount;
    g->path = 0;
    g->value = false;

    for (size_t i = 0; i < g->numberOfInputs; i++) {
        if (g->inputs[i] != NULL) {
            evaluate(err, g->inputs[i], callCount);
            if (*err == true) // jest cykl lub nie ma jakiegos wejscia
                return;
            if (g->inputs[i]->value == false)
                g->value = true; // jesli jakies wejscie g jest false to wartosc g jest true
            g->path = max(g->path, g->inputs[i]->path);
        }
        else if (g->boolInputs[i] == NULL) { // wejscie i jest puste
            *err = true;
            errno = ECANCELED;
            return;
        }
        else if (*g->boolInputs[i] == false) {
            g->value = true; // jesli jakies wejscie g jest false to wartosc g jest true
        }
    }
    if (g->numberOfInputs != 0) // jesli = 0 to bramka nie ma wejsc
        g->path++; // bo path = najwiekszy path z wejsc + 1
    g->lastChanged = callCount;
    return;
}

ssize_t nand_evaluate(nand_t** g, bool* s, size_t m) {
    if (g == NULL || s == NULL || m == 0) {
        errno = EINVAL;
        return -1;
    }
    static size_t callCount = -1;
    callCount++;
    bool err = false; // true jesli jest cykl/ktores wejscie jest puste

    for (size_t i = 0; i < m; i++) {
        if (g[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
        evaluate(&err, g[i], callCount);
        if (err == true)
            return -1;
    }
    size_t maxPath = 0;
    for (size_t i = 0; i < m; i++) {
        s[i] = g[i]->value;
        maxPath = max(maxPath, g[i]->path);
    }

    return maxPath;
}



