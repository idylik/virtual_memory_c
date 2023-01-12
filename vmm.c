#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "vmm.h"
#include "tlb.h"
#include "pt.h"
#include "pm.h"

static unsigned int read_count = 0;
static unsigned int write_count = 0;
static FILE* vmm_log;

static unsigned int sec_chance_count = 0;

void vmm_init (FILE *log)
{
  // Initialise le fichier de journal.
  vmm_log = log;
}


// NE PAS MODIFIER CETTE FONCTION
static void vmm_log_command (FILE *out, const char *command,
                             unsigned int laddress, /* Logical address. */
		             unsigned int page,
                             unsigned int frame,
                             unsigned int offset,
                             unsigned int paddress, /* Physical address.  */
		             char c) /* Caractère lu ou écrit.  */
{
  if (out)
    fprintf (out, "%s[%c]@%05d: p=%d, o=%d, f=%d pa=%d\n", command, c, laddress,
	     page, offset, frame, paddress);
}

//Retourne page_number dans PT, meilleure victime à remplacer dans PM
int vmm_choose_victim(void) {

    //Premier tour: essayer de trouver ref=0, readonly=1, changer ref bit -> 0
    //Deuxième tour: essayer de trouver ref=0, readonly=1, changer ref bit -> 0
    //Troisième tour: essayer de trouver ref=0, readonly=1, OU se contenter de ref=0, readonly=0

    //Tours 1,2,3
    for (int i = 0; i < 4; i++) {

        //Pages dans MEMOIRE PHYSIQUE
        for (int j=sec_chance_count; j < sec_chance_count+NUM_FRAMES; j++) {
            int f = j%NUM_FRAMES;
            int p = occupied_frames[f];
            //printf("f: %d\n",f);
            //printf("p: %d\n",p);
                //exit(0);
                bool ref = pt_ref_p(p);
                bool readonly =  pt_readonly_p(p);
                //printf("Search victim: p: %i, f: %i, ref: %i, readonly: %i\n", p, fn, ref, readonly);

                if ( ref == 0 && readonly == 1) { //Victime idéale
                    sec_chance_count++; //augmenter compteur: rotation des victimes
                    sec_chance_count = sec_chance_count % NUM_FRAMES;
                    return f;
                }

                //Si on est au 3e tour, on peut se contenter de ref=0, readonly=0, qu'on est sûr d'obtenir
                if (i==3 && ref == 0 && readonly == 0) {
                    sec_chance_count++; //augmenter compteur: rotation des victimes
                    sec_chance_count = sec_chance_count % NUM_FRAMES;
                    return f;
                }

                //Set reference bit à 0 (seconde chance):
                pt_set_ref(p, 0);
                //printf("Search victim AFTER: p: %i, f: %i, ref: %i, readonly: %i\n", p, fn, pt_ref_p(p), readonly);

        }
    }
    return 0;
}


/*Accède au frame en regardant d'abord dans la TLB, la PT, le BackStore
 * et fait les modifications nécessaires */
int access_frame(unsigned int page_number) {
    //Chercher dans TLB si le num_page s'y trouve:
    int frame_number = tlb_lookup(page_number, 1);

    if (frame_number < 0 ) { //Pas dans TLB, cherche dans page_table:

        frame_number = pt_lookup(page_number);

        if (frame_number < 0) { //PAGE FAULT, pas dans la mémoire physique -> downloader
            //printf("PAGE FAULT %d\n", page_number);
            //Chercher frame libre:
            for (int i=0; i < NUM_FRAMES; i++) {
                if (occupied_frames[i] < 0) {
                    frame_number = i;
                    break;
                }
            }

            if (frame_number < 0) { //Pas d'espace dans mémoire physique
                //Chercher frame victime avec Page Table
                frame_number = vmm_choose_victim();
                int p = occupied_frames[frame_number];
                //printf("Memoire physique: la victime est page %d, frame %d\n", p, frame_number);

                //Si victime a readonly=0 -> écrire la frame dans le backstore
                if (pt_readonly_p(p) == 0) {
                    pm_backup_page(frame_number,p);
                }
                //Victime n'est plus dans mémoire physique, désactiver dans PT et TLB!
                pt_unset_entry(p);
                tlb_unset_entry(p);
            }

            //Download page dans frame
            //Mettre à jour: frame_number dans pt, valid=1, readonly=1, ref=1
            pm_download_page(page_number, frame_number);
            pt_set_entry(page_number,frame_number);
            //sec_chance_count = (sec_chance_count+1) % NUM_PAGES;

        }
    }

    //Ajouter/updater cet accès à la TLB
    tlb_add_entry(page_number, frame_number,1);

    //Updater reference dans Page Table
    pt_set_ref(page_number,1);
    //printf("set ref to 1: page: %i, ref: %i\n", page_number, pt_ref_p(page_number));

    return frame_number;
}



/* Effectue une lecture à l'adresse logique `laddress`.  */
char vmm_read (unsigned int laddress)
{
    char c = '!';
    read_count++;


  /* ¡ TO DO: COMPLÉTER ! */

    unsigned int physical_address;

    //Extraire page_number et offset:
    unsigned int page_number = laddress >> 8;
    unsigned int offset = laddress  & 255;

    int frame_number = access_frame(page_number);

    physical_address = frame_number*PAGE_FRAME_SIZE+offset;

    //Lire caractère dans mémoire physique
    c = pm_read(physical_address);

  // TO DO: Fournir les arguments manquants.
  vmm_log_command (stdout, "READING", laddress, page_number, frame_number, offset, physical_address, c);
  return c;
}


/* Effectue une écriture à l'adresse logique `laddress`.  */
void vmm_write (unsigned int laddress, char c)
{
  write_count++;
  /* ¡ TO DO: COMPLÉTER ! */

    unsigned int physical_address;

    //Extraire page_number et offset:
    unsigned int page_number = laddress >> 8;
    unsigned int offset = laddress  & 255;

    int frame_number = access_frame(page_number);

    physical_address = frame_number*PAGE_FRAME_SIZE+offset;

    pm_write(physical_address, c);

    //Marquer page comme modifiée:
    pt_set_readonly(page_number, 0);

  // TO DO: Fournir les arguments manquants.
  vmm_log_command (stdout, "WRITING", laddress, page_number, frame_number, offset, physical_address, c);
}


// NE PAS MODIFIER CETTE FONCTION
void vmm_clean (void)
{
  fprintf (stdout, "VM reads : %4u\n", read_count);
  fprintf (stdout, "VM writes: %4u\n", write_count);
}
