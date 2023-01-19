#include <stdio.h>
#include <string.h>


#include "conf.h"
#include "pm.h"

static FILE *pm_backing_store;
static FILE *pm_log;
static unsigned int download_count = 0;
static unsigned int backup_count = 0;
static unsigned int read_count = 0;
static unsigned int write_count = 0;

// Initialise la mémoire physique
void pm_init (FILE *backing_store, FILE *log)
{
  pm_backing_store = backing_store;
  pm_log = log;
  memset (pm_memory, '\0', sizeof (pm_memory));

  for (int i=0; i < NUM_FRAMES; i++) {
      occupied_frames[i] = -1;
  }
}

// Charge la page demandée du backing store
void pm_download_page (unsigned int page_number, unsigned int frame_number)
{
  download_count++;


  unsigned int pos_pm = frame_number*PAGE_FRAME_SIZE;
  unsigned int pos_bs = page_number*PAGE_FRAME_SIZE;

  fseek(pm_backing_store, pos_bs, SEEK_SET);
  fread(&(pm_memory[pos_pm]), 1, PAGE_FRAME_SIZE, pm_backing_store);

  occupied_frames[frame_number] = page_number;
}

// Sauvegarde la frame spécifiée dans la page du backing store
void pm_backup_page (unsigned int frame_number, unsigned int page_number)
{
  backup_count++;
  
    unsigned int pos_pm = frame_number*PAGE_FRAME_SIZE;
    unsigned int pos_bs = page_number*PAGE_FRAME_SIZE;
    fseek(pm_backing_store, pos_bs, SEEK_SET);
    fwrite(&(pm_memory[pos_pm]), 1, PAGE_FRAME_SIZE, pm_backing_store);
    //readonly bit à 1 maintenant qu'on a backup
}

char pm_read (unsigned int physical_address)
{
  read_count++;
  return pm_memory[physical_address];
  return '!';
}

void pm_write (unsigned int physical_address, char c)
{
  write_count++;
    pm_memory[physical_address] = c;
}


void pm_clean (void)
{
  // Enregistre l'état de la mémoire physique.
  if (pm_log)
    {
      for (unsigned int i = 0; i < PHYSICAL_MEMORY_SIZE; i++)
	{
	  if (i % 80 == 0)
	    fprintf (pm_log, "%c\n", pm_memory[i]);
	  else
	    fprintf (pm_log, "%c", pm_memory[i]);
	}
    }
  fprintf (stdout, "Page downloads: %2u\n", download_count);
  fprintf (stdout, "Page backups  : %2u\n", backup_count);
  fprintf (stdout, "PM reads : %4u\n", read_count);
  fprintf (stdout, "PM writes: %4u\n", write_count);
}
