#include "LanguageModel.h"

#include <pthread.h>
#include <stdio.h>

#include <string>
#include <vector>

using LM::LanguageModel;
using std::vector;
using std::string;

const int CHECKING_COUNT = 50;
const int WORKER_COUNT = 23;
const int BUFFER_SIZE = 4096;
const char *log_file_prefix = "../data/worker.log.";
const char *index_file = "../data/lm.index";
const char *vocab_file = "../data/lm.vocab";

void *term_id_checking (void *ptr);

void trim(char *query)
{
    while ((*query != '\0') && (*query != '\r') && 
            (*query != '\n')) query++;
    *query = '\0';
}

struct st_para
{
    int id;
    LanguageModel *plm;
    vector<string> *pword_list;
};

LanguageModel *plm;
vector<string> word_list;

int main()
{
    /* load lm index */
    plm = new LanguageModel();
    if (plm == NULL) {
        fprintf(stderr,"Failed to create lm object.\n");
        return 1;
    }

    if (plm->load(index_file) != 0) {
        fprintf(stderr,"Failed to load lm index!\n");
        delete plm;
        return 2;
    }

    /* load vocabulary */
    char buffer[BUFFER_SIZE];
    FILE *fp_in = fopen(vocab_file,"r");
    while (fgets(buffer,sizeof(buffer),fp_in) != NULL) {
        trim(buffer);
        word_list.push_back(buffer);
    }
    fclose(fp_in);

    fprintf(stdout,"word list size = %d\n",word_list.size());
    fflush(stdout);

    pthread_t workers[WORKER_COUNT];
    /* initialize the workers */
    struct st_para para_list[WORKER_COUNT];
    for (size_t id = 0; id < WORKER_COUNT; id++) {
        para_list[id].id = id;
        para_list[id].plm = plm;
        para_list[id].pword_list = &word_list;
    }

    fprintf(stdout,"start to create workers...\n");
    for (size_t id = 0; id < WORKER_COUNT; id++) {
        pthread_create(&workers[id],NULL,term_id_checking,
                (void *)&para_list[id]);
    }

    for (size_t id = 0; id < WORKER_COUNT; id++) {
        pthread_join(workers[id],NULL);
    }
    fprintf(stdout,"all worker have quited...\n");

    /* delete memory */
    if (plm != NULL)
        delete plm;
}

void *term_id_checking (void *ptr)
{
    struct st_para *para = (struct st_para *)ptr;
    int id = para->id;
    LanguageModel *plm = para->plm;
    vector<string> *pword_list = para->pword_list;

    char strbuf[128];
    sprintf(strbuf,"%s%d",log_file_prefix,id);
    FILE *fp_log = fopen(strbuf,"w");
    for (size_t i = 0; i < CHECKING_COUNT; i++) {
        fprintf(fp_log,"the %dth checking...\n",i);
        for (size_t j = 0; j < pword_list->size(); j++) {
            const string &term = pword_list->at(j);
            uint32_t id = plm->getTermID(term);
            if (id != j + 1) {
                fprintf(fp_log,"%s\t%u\t%d\n",term.c_str(),id,j + 1);
            }
        }
    }
    fclose(fp_log);
}
