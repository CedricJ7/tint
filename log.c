#include <stdio.h>
#include <stdlib.h>

FILE *logfile;

void openlogfile ()
{
   if ((logfile = fopen ("/var/games/tint.log", "a+")) == NULL) {
	   fprintf (stderr, "Error creating logfile\n");
	   exit (EXIT_FAILURE);
   }
}

void closelogfile ()
{
   fclose (logfile);
}
