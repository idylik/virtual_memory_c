
#include <stdint.h>
#include <stdio.h>

#include "pt.h"

#include "conf.h"



static FILE *pt_log = NULL;

static unsigned int pt_lookup_count = 0;
static unsigned int pt_page_fault_count = 0;
static unsigned int pt_set_count = 0;

/* Initialise la table des pages, et indique où envoyer le log des accès.  */
void pt_init (FILE *log)
{
  for (unsigned int i; i < NUM_PAGES; i++){
      page_table[i].valid = false;
      page_table[i].ref = false;
    }

  pt_log = log;
}

/******************** ¡ NE RIEN CHANGER CI-DESSUS !  ******************/

/* Recherche dans la table des pages.
 * Renvoie le `frame_number`, si valide, ou un nombre négatif sinon.  */
static int pt__lookup (unsigned int page_number)
{
  // TO DO: COMPLÉTER CETTE FONCTION.
  if (page_table[page_number].valid) {
      return page_table[page_number].frame_number;
  }
  return -1;
}

/* Modifie l'entrée de `page_number` dans la page table pour qu'elle
 * pointe vers `frame_number`.  */
static void pt__set_entry (unsigned int page_number, unsigned int frame_number)
{

  page_table[page_number].frame_number = frame_number;
  page_table[page_number].valid = 1;
  pt_set_ref(page_number,1);
  pt_set_readonly(page_number,1);
}

/* Marque l'entrée de `page_number` dans la page table comme invalide.  */
void pt_unset_entry (unsigned int page_number)
{
  page_table[page_number].valid = 0;
    //Unset aussi dans TLB
}

/* Renvoie si `page_number` est `readonly`.  */
bool pt_readonly_p (unsigned int page_number)
{
  if (page_table[page_number].readonly == 0) return false;
  return true;
}

/* Change l'accès en écriture de `page_number` selon `readonly`.  */
void pt_set_readonly (unsigned int page_number, bool readonly)
{
  page_table[page_number].readonly = readonly;
}

/* Renvoie si `page_number` est `ref`.  */
bool pt_ref_p (unsigned int page_number)
{
    if (page_table[page_number].ref == 0) return false;
    return true;
}

/* Change l'accès en écriture de `page_number` selon `ref`.  */
void pt_set_ref (unsigned int page_number, bool ref)
{

    page_table[page_number].ref = ref;
}


/******************** ¡ NE RIEN CHANGER CI-DESSOUS !  ******************/

void pt_set_entry (unsigned int page_number, unsigned int frame_number)
{
  pt_set_count++;
  pt__set_entry (page_number, frame_number);
}

int pt_lookup (unsigned int page_number)
{
  pt_lookup_count++;
  int fn = pt__lookup (page_number);
  if (fn < 0) pt_page_fault_count++;
  return fn;
}

/* Imprime un sommaires des accès.  */
void pt_clean (void)
{
  fprintf (stdout, "PT lookups   : %3u\n", pt_lookup_count);
  fprintf (stdout, "PT changes   : %3u\n", pt_set_count);
  fprintf (stdout, "Page Faults  : %3u\n", pt_page_fault_count);

  if (pt_log)
  {
    for (unsigned int i = 0; i < NUM_PAGES; i++)
    {
      fprintf (pt_log,
          "%d -> {%d,%s%s}\n",
          i,
          page_table[i].frame_number,
          page_table[i].valid ? "" : " invalid",
          page_table[i].readonly ? " readonly" : "");
    }
  }
}
