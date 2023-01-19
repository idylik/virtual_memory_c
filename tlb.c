
#include <stdint.h>
#include <stdio.h>

#include "tlb.h"

#include "conf.h"

struct tlb_entry
{
  unsigned int page_number;
  int frame_number;             /* Invalide si négatif.  */
  bool readonly : 1;    //Sert de reference bit seulement
};

static FILE *tlb_log = NULL;
static struct tlb_entry tlb_entries[TLB_NUM_ENTRIES];

static unsigned int tlb_hit_count = 0;
static unsigned int tlb_miss_count = 0;
static unsigned int tlb_mod_count = 0;
//Ajout
static unsigned int tlb_sec_chance_count = 0;

/* Initialise le TLB, et indique où envoyer le log des accès.  */
void tlb_init (FILE *log)
{
  for (int i = 0; i < TLB_NUM_ENTRIES; i++)
    tlb_entries[i].frame_number = -1;
  tlb_log = log;
}

/******************** ¡ NE RIEN CHANGER CI-DESSUS !  ******************/

/* Recherche dans le TLB.
 * Renvoie le `frame_number`, si trouvé, ou un nombre négatif sinon.  */
static int tlb__lookup (unsigned int page_number, bool write)
{
  for (int i=0; i < TLB_NUM_ENTRIES; i++) {

      if (tlb_entries[i].frame_number != -1 && tlb_entries[i].page_number == page_number) {
          tlb_entries[i].readonly = write;
          return tlb_entries[i].frame_number;
      }
  }
  return -1;
}

/* Ajoute dans le TLB une entrée qui associe `frame_number` à
 * `page_number`.  */
static void tlb__add_entry (unsigned int page_number,
                            unsigned int frame_number, bool readonly)
{

    //Chaque fois qu'on ajoute dans Page Table, on ajoute ENSUITE dans TLB.
    //On ajoute readonly=1 chaque fois que la page est accédée

  //Si page number déjà présent, mettre à jour:
  for (int i=0; i < TLB_NUM_ENTRIES; i++) {
      if (tlb_entries[i].frame_number != -1 && tlb_entries[i].page_number == page_number) {
          //mettre à jour:
          tlb_entries[i].frame_number = frame_number;
          tlb_entries[i].readonly = readonly;
          return;
      }
  }


  //Sinon, placer dans un espace vide:
    for (int i=0; i < TLB_NUM_ENTRIES; i++) {
        if (tlb_entries[i].frame_number == -1) {
            tlb_entries[i].page_number = page_number;
            tlb_entries[i].frame_number = frame_number;
            tlb_entries[i].readonly = readonly;
            return;
        }
    }



    //Ici, le bit readonly sert juste pour second chance
    //Sinon, algo Second chance: Trouver une victime,
    //Choisir une entrée avec readonly=0;
    //Setter les entrées parcourues readonly=1 à readonly=0
    //Pas besoin d'écrire dans la mémoire physique, l'entrée se trouve dans la PT

    while(tlb_entries[tlb_sec_chance_count].readonly != 0) {
        tlb_entries[tlb_sec_chance_count].readonly = 0;
        tlb_sec_chance_count++;
        tlb_sec_chance_count = tlb_sec_chance_count % TLB_NUM_ENTRIES;
    }

    //tlb_sec_chance_count est l'index de la victime
    tlb_entries[tlb_sec_chance_count].page_number = page_number;
    tlb_entries[tlb_sec_chance_count].frame_number = frame_number;
    tlb_entries[tlb_sec_chance_count].readonly = readonly;
    return;

}

void tlb_unset_entry(unsigned int page_number) {

    for (int i=0; i < TLB_NUM_ENTRIES; i++) {
        if (tlb_entries[i].page_number == page_number) {
            tlb_entries[i].frame_number = -1;
        }
    }
}

/******************** ¡ NE RIEN CHANGER CI-DESSOUS !  ******************/

void tlb_add_entry (unsigned int page_number,
                    unsigned int frame_number, bool readonly)
{
  tlb_mod_count++;
  tlb__add_entry (page_number, frame_number, readonly);
}

int tlb_lookup (unsigned int page_number, bool write)
{
  int fn = tlb__lookup (page_number, write);
  (*(fn < 0 ? &tlb_miss_count : &tlb_hit_count))++;
  return fn;
}

/* Imprime un sommaires des accès.  */
void tlb_clean (void)
{
  fprintf (stdout, "TLB misses   : %3u\n", tlb_miss_count);
  fprintf (stdout, "TLB hits     : %3u\n", tlb_hit_count);
  fprintf (stdout, "TLB changes  : %3u\n", tlb_mod_count);
  fprintf (stdout, "TLB miss rate: %.1f%%\n",
           100 * tlb_miss_count
           /* Ajoute 0.01 pour éviter la division par 0.  */
           / (0.01 + tlb_hit_count + tlb_miss_count));
}
