/*
 * Implementation of the word_count interface using Pintos lists.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t* wclist) { /* TODO */
  list_init(wclist);
}

size_t len_words(word_count_list_t* wclist) {

  uint32_t count = 0U;
  struct list_elem* e;
  for (e = list_begin(wclist); e != list_end(wclist); e = list_next(e))
  {
    count++;
  }
  return count;
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {

  struct list_elem* e;
  for (e = list_begin(wclist); e != list_end(wclist); e = list_next(e))
  {
    word_count_t* ptr = list_entry(e, struct word_count, elem);
    if (!strcmp(ptr->word, word))
        return ptr;
  }

  return NULL;
}

char *new_string(char *str) {
  char *new_str = (char *) malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {

  word_count_t* ptr;
  ptr = find_word(wclist, word);
  if (ptr)
  {
    ptr->count++;
    return ptr;
  }
  else
  {
    word_count_t* wc = (word_count_t*)malloc(sizeof(word_count_t));
    wc->word = new_string(word);
    //wc->word = word;
    wc->count = 1;
    list_insert(list_begin(wclist), &(wc->elem)); 
    return wc;
  }
  return NULL;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) { 

  struct list_elem* e;
  for (e = list_begin(wclist); e != list_end(wclist); e = list_next(e))
  {
    word_count_t* ptr = list_entry(e, struct word_count, elem);
    fprintf(outfile, "    %d    %s\n", ptr->count, ptr->word);
  }

}

static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {

  word_count_t* ptr1 = list_entry(ewc1, struct word_count, elem);
  word_count_t* ptr2 = list_entry(ewc2, struct word_count, elem);

  //if (ptr1->count > ptr2->count)
  //{
  //  return false;
  //}
  //else if(ptr1->count < ptr2->count)
  //{
  //  return true;
  //}
  //else
  //{
  //  if (strcmp(ptr1->word, ptr2->word) <= 0)
  //  {
  //    return true;
  //  }
  //  else
  //  {
  //    return false;
  //  }
  //}
  bool (*ptr) (const word_count_t*, const word_count_t*) = aux;
  return (*ptr)(ptr1, ptr2);
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less_word(const word_count_t*, const word_count_t*)) {
  list_sort(wclist, less_list, less_word);
}
