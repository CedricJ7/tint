/*
 * Copyright (c) Abraham vd Merwe <abz@blio.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "typedefs.h"
#include "utils.h"
#include "io.h"
#include "config.h"
#include "engine.h"
#include "log.h"

/*
 * Macros
 */

/* Upper left corner of board */
#define XTOP ((out_width () - NUMROWS - 3) >> 1)
#define YTOP ((out_height () - NUMCOLS - 9) >> 1)

/* Maximum digits in a number (i.e. number of digits in score, */
/* number of blocks, etc. should not exceed this value */
#define MAXDIGITS 12

/* Number of levels in the game */
#define MINLEVEL	1
#define MAXLEVEL	9

/* This calculates the time allowed to move a shape, before it is moved a row down */
#define DELAY (1000000 / (level + 2))

/* The score is multiplied by this to avoid losing precision */
#define SCOREFACTOR 2

/* This calculates the stored score value */
#define SCOREVAL(x) (SCOREFACTOR * (x))

/* This calculates the real (displayed) value of the score */
#define GETSCORE(score) ((score) / SCOREFACTOR)

/* Length of a player's name */
#define NAMELEN 20

static bool shownext;
static bool dottedlines;
static bool shadow;
static bool newturn;
static int level = MINLEVEL - 1,shapecount[NUMSHAPES], turn = 0;
static char blockchar = ' ';
static char playername[NAMELEN] = ""; // Variable globale pour le nom du joueur
static int scoressize = 0;

/*
 * Functions
 */

// Déclaration de la fonction get_timestamp_string
static void get_timestamp_string(char *buffer, size_t buffer_size);

/* This function is responsible for increasing the score appropriately whenever
 * a block collides at the bottom of the screen (or the top of the heap */
static void score_function (engine_t *engine)
{
   int score = SCOREVAL (level * (engine->status.dropcount + 1));
   score += SCOREVAL ((level + 10) * engine->status.currentdroppedlines * engine->status.currentdroppedlines);

   if (shownext) score /= 2;
   if (dottedlines) score /= 2;

   engine->score += score;
}

/* Draw the board on the screen */
static void drawboard (board_t board)
{
   int x,y;
   out_setattr (ATTR_OFF);
   for (y = 1; y < NUMROWS - 1; y++) for (x = 0; x < NUMCOLS - 1; x++)
	 {
		out_gotoxy (XTOP + x * 2,YTOP + y);
		switch (board[x][y])
		  {
			 /* Wall */
		   case WALL:
			 out_setattr (ATTR_BOLD);
			 out_setcolor (COLOR_BLUE,COLOR_BLACK);
			 out_putch ('<');
			 out_putch ('>');
			 out_setattr (ATTR_OFF);   
			 break;
			 /* Background */
		   case 0:
			 if (dottedlines)
			   {
				  out_setcolor (COLOR_BLUE,COLOR_BLACK);
				  out_putch ('.');
				  out_putch (' ');
			   }
			 else
			   {
				  out_setcolor (COLOR_BLACK,COLOR_BLACK);
				  out_putch (' ');
				  out_putch (' ');
			   }
			 break;
			 /* Block */
		   default:
			 out_setcolor (COLOR_BLACK,board[x][y]);
			 out_putch (blockchar);
			 out_putch (blockchar);
		  }
	 }
   out_setattr (ATTR_OFF);
}

/* Show the next piece on the screen */
static void drawnext (int shapenum,int x,int y)
{
   int i;
   block_t ofs[NUMSHAPES] =
	 { { 1,  0 }, { 1,  0 }, { 1, -1 }, { 2,  0 }, { 1, -1 }, { 1, -1 }, { 0, -1 } };
   out_setcolor (COLOR_BLACK,COLOR_BLACK);
   for (i = y - 2; i < y + 2; i++)
	 {
		out_gotoxy (x - 2,i);
		out_printf ("        ");
	 }
   out_setcolor (COLOR_BLACK,SHAPES[shapenum].color);
   for (i = 0; i < NUMBLOCKS; i++)
	 {
		out_gotoxy (x + SHAPES[shapenum].block[i].x * 2 + ofs[shapenum].x,
					y + SHAPES[shapenum].block[i].y + ofs[shapenum].y);
		out_putch (' ');
		out_putch (' ');
	 }
}

/* Draw the background */
static void drawbackground ()
{
   out_setattr (ATTR_OFF);
   out_setcolor (COLOR_WHITE,COLOR_BLACK);
   out_gotoxy (4,YTOP + 7);   out_printf ("H E L P");
   out_gotoxy (1,YTOP + 9);   out_printf ("p: Pause");
   out_gotoxy (1,YTOP + 10);  out_printf ("j: Left");
   out_gotoxy (1,YTOP + 11);  out_printf ("l: Right");
   out_gotoxy (1,YTOP + 12);  out_printf ("k: Rotate");
   out_gotoxy (1,YTOP + 13);  out_printf ("s: Draw next");
   out_gotoxy (1,YTOP + 14);  out_printf ("d: Toggle lines");
   out_gotoxy (1,YTOP + 15);  out_printf ("a: Speed up");
   out_gotoxy (1,YTOP + 16);  out_printf ("q: Quit");
   out_gotoxy (2,YTOP + 17);  out_printf ("SPACE: Drop");
   out_gotoxy (3,YTOP + 19);  out_printf ("Next:");
}

static int getsum ()
{
   int i,sum = 0;
   for (i = 0; i < NUMSHAPES; i++) sum += shapecount[i];
   return (sum);
}

/* This show the current status of the game */
static void showstatus (engine_t *engine)
{
   static const int shapenum[NUMSHAPES] = { 4, 6, 5, 1, 0, 3, 2 };
   char tmp[MAXDIGITS + 1];
   char timestamp_str[32];
   int i,sum = getsum ();
   
   out_setattr (ATTR_OFF);
   out_setcolor (COLOR_WHITE,COLOR_BLACK);
   out_gotoxy (1,YTOP + 1);   out_printf ("Your level: %d",level);
   out_gotoxy (1,YTOP + 2);   out_printf ("Full lines: %d",engine->status.droppedlines);
   out_gotoxy (1,YTOP + 3);   out_printf ("curx : %d, cury :%d", engine->curx,engine->cury);
   out_gotoxy (2,YTOP + 4);   out_printf ("Score");
   out_setattr (ATTR_BOLD);
   out_setcolor (COLOR_YELLOW,COLOR_BLACK);
   out_printf ("  %d",GETSCORE (engine->score));
   if (shownext) drawnext (engine->nextshape,3,YTOP + 22);
   out_setattr (ATTR_OFF);
   out_setcolor (COLOR_WHITE,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 12,YTOP + 1);
   out_printf ("STATISTICS");
   out_setcolor (COLOR_BLACK,COLOR_MAGENTA);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 3);
   out_printf ("      ");
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 4);
   out_printf ("  ");
   out_setcolor (COLOR_MAGENTA,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 3);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[0]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 3);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_BLACK,COLOR_RED);
   out_gotoxy (out_width () - MAXDIGITS - 13,YTOP + 5);
   out_printf ("        ");
   out_setcolor (COLOR_RED,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 5);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[1]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 5);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_BLACK,COLOR_WHITE);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 7);
   out_printf ("      ");
   out_gotoxy (out_width () - MAXDIGITS - 13,YTOP + 8);
   out_printf ("  ");
   out_setcolor (COLOR_WHITE,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 7);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[2]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 7);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_BLACK,COLOR_GREEN);
   out_gotoxy (out_width () - MAXDIGITS - 9,YTOP + 9);
   out_printf ("    ");
   out_gotoxy (out_width () - MAXDIGITS - 11,YTOP + 10);
   out_printf ("    ");
   out_setcolor (COLOR_GREEN,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 9);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[3]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 9);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_BLACK,COLOR_CYAN);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 11);
   out_printf ("    ");
   out_gotoxy (out_width () - MAXDIGITS - 15,YTOP + 12);
   out_printf ("    ");
   out_setcolor (COLOR_CYAN,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 11);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[4]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 11);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_BLACK,COLOR_BLUE);
   out_gotoxy (out_width () - MAXDIGITS - 9,YTOP + 13);
   out_printf ("    ");
   out_gotoxy (out_width () - MAXDIGITS - 9,YTOP + 14);
   out_printf ("    ");
   out_setcolor (COLOR_BLUE,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 13);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[5]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 13);
   out_printf ("%s",tmp);
   out_setattr (ATTR_OFF);
   out_setcolor (COLOR_BLACK,COLOR_YELLOW);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 15);
   out_printf ("      ");
   out_gotoxy (out_width () - MAXDIGITS - 15,YTOP + 16);
   out_printf ("  ");
   out_setcolor (COLOR_YELLOW,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 3,YTOP + 15);
   out_putch ('-');
   snprintf (tmp,MAXDIGITS + 1,"%d",shapecount[shapenum[6]]);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 15);
   out_printf ("%s",tmp);
   out_setcolor (COLOR_WHITE,COLOR_BLACK);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 17);
   for (i = 0; i < MAXDIGITS + 16; i++) out_putch ('-');
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 18);
   out_printf ("Sum          :");
   snprintf (tmp,MAXDIGITS + 1,"%d",sum);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 18);
   out_printf ("%s",tmp);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 20);
   for (i = 0; i < MAXDIGITS + 16; i++) out_putch (' ');
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 20);
   out_printf ("Score ratio  :");
   snprintf (tmp,MAXDIGITS + 1,"%d",GETSCORE (engine->score) / sum);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 20);
   out_printf ("%s",tmp);
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 21);
   for (i = 0; i < MAXDIGITS + 16; i++) out_putch (' ');
   out_gotoxy (out_width () - MAXDIGITS - 17,YTOP + 21);
   out_printf ("Efficiency   :");
   snprintf (tmp,MAXDIGITS + 1,"%d",engine->status.efficiency);
   out_gotoxy (out_width () - strlen (tmp) - 1,YTOP + 21);
   out_printf ("%s",tmp);

   if (newturn)
   {
       get_timestamp_string(timestamp_str, sizeof(timestamp_str));
       fprintf(logfile, "%s Level = %d\n", timestamp_str, level);
       fprintf(logfile, "%s Score = %d\n", timestamp_str, GETSCORE (engine->score));
       fprintf(logfile, "%s Full lines = %d\n", timestamp_str, engine->status.droppedlines);
       fprintf(logfile, "%s Current shape position: x=%d, y=%d\n", timestamp_str, engine->curx, engine->cury);
       fprintf(logfile, "%s STATISTICS\n", timestamp_str);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[4], shapecount[shapenum[4]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[3], shapecount[shapenum[3]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[6], shapecount[shapenum[6]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[5], shapecount[shapenum[5]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[0], shapecount[shapenum[0]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[2], shapecount[shapenum[2]]);
       fprintf(logfile, "%s Shape(%d) = %d\n", timestamp_str, shapenum[1], shapecount[shapenum[1]]);
       fprintf(logfile, "%s Sum = %d\n", timestamp_str, sum);
       fprintf(logfile, "%s Score ratio = %d\n", timestamp_str, GETSCORE (engine->score) / sum);
       fprintf(logfile, "%s Efficiency = %d\n", timestamp_str, engine->status.efficiency);
       newturn = FALSE;
   }

}

          /***************************************************************************/
          /***************************************************************************/
          /***************************************************************************/

/* Header for scorefile */
#define SCORE_HEADER	"Tint 0.02b (c) Abraham vd Merwe - Scores"

/* Header for score title */
static const char scoretitle[] = "\n\t   TINT HIGH SCORES\n\n\tRank   Score        Name\n\n";


/* Number of scores allowed in highscore list */
#define TOP_SCORES 10

typedef struct
{
   char name[NAMELEN];
   int score;
   time_t timestamp;
} score_t;

static void getname (char *name)
{
   struct passwd *pw = getpwuid (geteuid ());

   fprintf (stderr,"Enter your name [%s]: ",pw != NULL ? pw->pw_name : "");

   fgets (name,NAMELEN - 1,stdin);
   name[strlen (name) - 1] = '\0';

   if (!strlen (name) && pw != NULL)
     {
        strncpy (name,pw->pw_name,NAMELEN);
        name[NAMELEN - 1] = '\0';
     }
}

// Update the timestamp function to use real calendar time with proper formatting
static void get_timestamp_string(char *buffer, size_t buffer_size)
{
   struct timeval tv;
   struct tm *tm_info;
   
   gettimeofday(&tv, NULL);
   tm_info = localtime(&tv.tv_sec);
   
   snprintf(buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec / 1000);
}

// Fonction pour demander le nom au début du jeu
static void get_player_name()
{
   // D'abord demander le nom à l'utilisateur
   getname(playername);
}

static void err1 ()
{
   fprintf (stderr,"Error creating %s\n",scorefile);
   exit (EXIT_FAILURE);
}

static void err2 ()
{
   fprintf (stderr,"Error writing to %s\n",scorefile);
   exit (EXIT_FAILURE);
}

void showplayerstats (engine_t *engine)
{
   fprintf (stderr,
			"\n\t   PLAYER STATISTICS\n\n\t"
			"Score       %11d\n\t"
			"Efficiency  %11d\n\t"
			"Score ratio %11d\n",
			GETSCORE (engine->score),engine->status.efficiency,GETSCORE (engine->score) / getsum ());
}

static void createscores (int score)
{
   FILE *handle;
   int i,j;
   score_t *scores = malloc(scoressize * sizeof(score_t));
   if (!scores) { fprintf(stderr, "Erreur allocation mémoire\n"); exit(1); }
   if (score == 0) return;	/* No need saving this */
   for (i = 1; i < scoressize; i++)
     {
        strcpy (scores[i].name,"None");
        scores[i].score = -1;
        scores[i].timestamp = 0;
     }
   // Utiliser le nom déjà saisi au lieu de le redemander
   strcpy(scores[scoressize - 1].name, playername);
   scores[scoressize - 1].score = score;
   scores[scoressize - 1].timestamp = time(NULL);
   if ((handle = fopen (scorefile,"w")) == NULL) err1 ();
   if (i != 1) err2 ();
   for (i = 0; i < scoressize; i++)
     {
        j = fwrite (scores[i].name,strlen (scores[i].name) + 1,1,handle);
        if (j != 1) err2 ();
        j = fwrite (&(scores[i].score),sizeof (int),1,handle);
        if (j != 1) err2 ();
        j = fwrite (&(scores[i].timestamp),sizeof (time_t),1,handle);
        if (j != 1) err2 ();
     }
   fclose (handle);

   fprintf (stderr,"%s",scoretitle);
   fprintf (stderr,"\t  1* %7d        %s\n\n",score,scores[0].name);
}

static int cmpscores (const void *a,const void *b)
{
   int result;
   result = (int) ((score_t *) a)->score - (int) ((score_t *) b)->score;
   /* a < b */
   if (result < 0) return 1;
   /* a > b */
   if (result > 0) return -1;
   /* a = b */
   result = (time_t) ((score_t *) a)->timestamp - (time_t) ((score_t *) b)->timestamp;
   /* a is older */
   if (result < 0) return -1;
   /* b is older */
   if (result > 0) return 1;
   /* timestamps is equal */
   return 0;
}

static void savescores (int score)
{
   FILE *handle;
   int i,j,ch;
   score_t *scores = malloc(scoressize * sizeof(score_t));
   if (!scores) { fprintf(stderr, "Erreur allocation mémoire\n"); exit(1); }
   time_t tmp = 0;
   char timestamp_str[32];
   if ((handle = fopen (scorefile,"r")) == NULL)
     {
        createscores (score);
        return;
     }
   if (i != 1)
     {
        createscores (score);
        return;
     }
   for (i = 0; i < scoressize; i++)
     {
        j = 0;
        while ((ch = fgetc (handle)) != '\0')
          {
             if ((ch == EOF) || (j >= NAMELEN - 2))
               {
                  createscores (score);
                  return;
               }
             scores[i].name[j++] = (char) ch;
          }
        scores[i].name[j] = '\0';
        j = fread (&(scores[i].score),sizeof (int),1,handle);
        if (j != 1)
          {
             createscores (score);
             return;
          }
        j = fread (&(scores[i].timestamp),sizeof (time_t),1,handle);
        if (j != 1)
          {
             createscores (score);
             return;
          }
     }
   fclose (handle);

   // Utiliser le nom déjà saisi au lieu de le redemander
   strcpy(scores[scoressize - 1].name, playername);
   scores[scoressize - 1].score = score;
   scores[scoressize - 1].timestamp = tmp = time (NULL);
     
   if ((handle = fopen (scorefile,"w")) == NULL) err2 ();
   if (i != 1) err2 ();
   for (i = 0; i < scoressize; i++)
     {
        j = fwrite (scores[i].name,strlen (scores[i].name) + 1,1,handle);
        if (j != 1) err2 ();
        j = fwrite (&(scores[i].score),sizeof (int),1,handle);
        if (j != 1) err2 ();
        j = fwrite (&(scores[i].timestamp),sizeof (time_t),1,handle);
        if (j != 1) err2 ();
     }
   fclose (handle);

   fprintf (stderr,"%s",scoretitle);
   i = 0;
   while ((i < scoressize) && (scores[i].score != -1))
     {
        j = scores[i].timestamp == tmp ? '*' : ' ';
        fprintf (stderr,"\t %2d%c %7d        %s\n",i + 1,j,scores[i].score,scores[i].name);
        i++;
     }
   fprintf (stderr,"\n");

   /* LOG */
   get_timestamp_string(timestamp_str, sizeof(timestamp_str));
   fprintf(logfile, "%s ~~ FINAL SCORE ~~\n", timestamp_str);
   fprintf(logfile, "%s Player name = %s\n", timestamp_str, scores[TOP_SCORES
   - 1].name);
   fprintf(logfile, "%s Player score = %7d\n", timestamp_str, scores[TOP_SCORES
   - 1].score);
   fprintf(logfile, "%s Player timestamp = %ld\n", timestamp_str, scores[TOP_SCORES
   - 1].timestamp);
   fprintf(logfile, "%s ~~~~~~~~~~~~~~~~\n", timestamp_str);
}

          /***************************************************************************/
          /***************************************************************************/
          /***************************************************************************/

static void showhelp ()
{
   fprintf (stderr,"USAGE: tint [-h] [-l level] [-n] [-d] [-b char]\n");
   fprintf (stderr,"  -h           Show this help message\n");
   fprintf (stderr,"  -l <level>   Specify the starting level (%d-%d)\n",MINLEVEL,MAXLEVEL);
   fprintf (stderr,"  -n           Draw next shape\n");
   fprintf (stderr,"  -d           Draw vertical dotted lines\n");
   fprintf (stderr,"  -b <char>    Use this character to draw blocks instead of spaces\n");
   fprintf (stderr,"  -s           Draw shadow of shape\n");
   exit (EXIT_FAILURE);
}

static void parse_options (int argc,char *argv[])
{
   int i = 1;
   while (i < argc)
	 {
		/* Help? */
		if (strcmp (argv[i],"-h") == 0)
		  showhelp ();
		/* Level? */
		else if (strcmp (argv[i],"-l") == 0)
		  {
			 i++;
			 if (i >= argc || !str2int (&level,argv[i])) showhelp ();
			 if ((level < MINLEVEL) || (level > MAXLEVEL))
			   {
				  fprintf (stderr,"You must specify a level between %d and %d\n",MINLEVEL,MAXLEVEL);
				  exit (EXIT_FAILURE);
			   }
		  }
		/* Show next? */
		else if (strcmp (argv[i],"-n") == 0)
		  shownext = TRUE;
		else if(strcmp(argv[i],"-d")==0)
		  dottedlines = TRUE;
		else if(strcmp(argv[i], "-b")==0)
		  {
		    i++;
		    if (i >= argc || strlen(argv[i]) < 1) showhelp();
		    blockchar = argv[i][0];
		  }
		else if (strcmp (argv[i],"-s") == 0)
            shadow = TRUE;
		else
		  {
			 fprintf (stderr,"Invalid option -- %s\n",argv[i]);
			 showhelp ();
		  }
		i++;
	 }
}

static void choose_level ()
{
   char buf[NAMELEN];

   do
	 {
		fprintf (stderr,"Choose a level to start [%d-%d]: ",MINLEVEL,MAXLEVEL);
		fgets (buf,NAMELEN - 1,stdin);
		buf[strlen (buf) - 1] = '\0';
	 }
   while (!str2int (&level,buf) || level < MINLEVEL || level > MAXLEVEL);
}

static bool evaluate (engine_t *engine)
{
    bool finished = FALSE;
    char timestamp_str[32];
    
    switch (engine_evaluate (engine))
    {
        /* game over (board full) */
        case -1:
            if ((level < MAXLEVEL) && ((engine->status.droppedlines / 10) > level)) level++;
            finished = TRUE;
            get_timestamp_string(timestamp_str, sizeof(timestamp_str));
            fprintf(logfile, "%s GAME FINISHED at position: shape %d at (x=%d, y=%d)\n", 
                    timestamp_str, engine->curshape, engine->curx, engine->cury);
            fprintf(logfile, "%s Cause: Board full (game over)\n", timestamp_str);
        break;
            /* shape at bottom, next one released */
        case 0:
            if ((level < MAXLEVEL) && ((engine->status.droppedlines / 10) > level))
            {
                level++;
                in_timeout (DELAY);
            }
            shapecount[engine->curshape]++;
            get_timestamp_string(timestamp_str, sizeof(timestamp_str));
            fprintf(logfile, "%s Shape %d landed at final position (x=%d, y=%d)\n", 
                    timestamp_str, engine->curshape, engine->curx, engine->cury);
            
            if (engine->status.currentdroppedlines > 0) {
                fprintf(logfile, "%s Lines cleared: %d lines\n", timestamp_str, 
                        engine->status.currentdroppedlines);
            }
            
            fprintf(logfile, "%s Turn[%d] = Finished\n", timestamp_str, turn);
            newturn = TRUE;
            turn++;
            break;
            /* shape moved down one line */
        case 1:
            get_timestamp_string(timestamp_str, sizeof(timestamp_str));
            fprintf(logfile, "%s Shape %d moved down to (x=%d, y=%d)\n", 
                    timestamp_str, engine->curshape, engine->curx, engine->cury);
        break;
    }
    return finished;
}

          /***************************************************************************/
          /***************************************************************************/
          /***************************************************************************/

int main (int argc,char *argv[])
{
   bool finished;
   int ch;
   engine_t engine;
   char timestamp_str[32];
   
   /* Demander le nom du joueur en premier */
   get_player_name();
   
   /* Initialize */
   newturn = TRUE;
   rand_init ();							/* must be called before engine_init () */
   engine_init (&engine,score_function);	/* must be called before using engine.curshape */
   finished = shownext = shadow = FALSE;
   memset (shapecount,0,NUMSHAPES * sizeof (int));
   shapecount[engine.curshape]++;
   parse_options (argc,argv);				/* must be called after initializing variables */
   engine.shadow = shadow;
   if (level < MINLEVEL) choose_level ();
   io_init ();
   /* Open log file */
   openlogfile();
   
   /* Log game start info with proper format */
   get_timestamp_string(timestamp_str, sizeof(timestamp_str));
   fprintf(logfile, "GAME STARTED at timestamp = %s\n", timestamp_str);
   fprintf(logfile, "Player name: %s\n", playername);
   fprintf(logfile, "Starting level: %d\n", level);
   fprintf(logfile, "Game options: shownext=%s, dottedlines=%s, shadow=%s\n", 
           shownext ? "true" : "false", dottedlines ? "true" : "false", shadow ? "true" : "false");
   fprintf(logfile, "Block character: '%c'\n", blockchar);
   
   drawbackground ();
   in_timeout (DELAY);
   /* Main loop */
   do
     {
         if (newturn) {
             get_timestamp_string(timestamp_str, sizeof(timestamp_str));
             fprintf(logfile, "Turn[%d] = Started\n", turn);
             fprintf(logfile, "New shape(%d) spawned at (x=%d, y=%d)\n", 
                     engine.curshape, engine.curx, engine.cury);
             fprintf(logfile, "Turn[%d] timestamp = %s\n", turn, timestamp_str);
             fprintf(logfile, "Level = %d\n", level);
             fprintf(logfile, "Score = %d\n", GETSCORE (engine.score));
             fprintf(logfile, "Full lines = %d\n", engine.status.droppedlines);
             fprintf(logfile, "Current shape = %d at position (x=%d, y=%d)\n", 
                     engine.curshape, engine.curx, engine.cury);
             fprintf(logfile, "Next shape = %d\n", engine.nextshape);
             fprintf(logfile, "Drop count this turn = %d\n", engine.status.dropcount);
             fprintf(logfile, "Lines dropped this turn = %d\n", engine.status.currentdroppedlines);
             fprintf(logfile, "STATISTICS\n");
             for (int i = 0; i < NUMSHAPES; i++) {
                 fprintf(logfile, "Shape(%d) = %d\n", i, shapecount[i]);
             }
             fprintf(logfile, "Sum = %d\n", getsum());
             fprintf(logfile, "Score ratio = %d\n", GETSCORE (engine.score) / getsum());
             fprintf(logfile, "Efficiency = %d\n", engine.status.efficiency);
             newturn = FALSE;
        }
		/* draw shape */
		showstatus (&engine);
		drawboard (engine.board);
		out_refresh ();
		/* Check if user pressed a key */
		if ((ch = in_getch ()) != ERR)
		  {
			 switch (ch)
			   {
				case 'j':
				case KEY_LEFT:
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s ACTION: Move LEFT from (x=%d, y=%d)\n", 
				          timestamp_str, engine.curx, engine.cury);
				  engine_move (&engine,ACTION_LEFT);
				  fprintf(logfile, "Result: shape now at (x=%d, y=%d)\n", 
				          engine.curx, engine.cury);
				  break;
				case 'k':
				case KEY_UP:
				case '\n':
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s ACTION: ROTATE shape %d at (x=%d, y=%d)\n", 
				          timestamp_str, engine.curshape, engine.curx, engine.cury);
				  engine_move (&engine,ACTION_ROTATE);
				  fprintf(logfile, "Result: shape now at (x=%d, y=%d)\n", 
				          engine.curx, engine.cury);
				  break;
				case 'l':
				case KEY_RIGHT:
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s ACTION: Move RIGHT from (x=%d, y=%d)\n", 
				          timestamp_str, engine.curx, engine.cury);
				  engine_move (&engine,ACTION_RIGHT);
				  fprintf(logfile, "Result: shape now at (x=%d, y=%d)\n", 
				          engine.curx, engine.cury);
				  break;
				case KEY_DOWN:
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s ACTION: Move DOWN from (x=%d, y=%d)\n", 
				          timestamp_str, engine.curx, engine.cury);
				  engine_move (&engine,ACTION_DOWN);
				  fprintf(logfile, "Result: shape now at (x=%d, y=%d)\n", 
				          engine.curx, engine.cury);
				  break;
				case ' ':
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s ACTION: DROP shape %d from (x=%d, y=%d)\n", 
				          timestamp_str, engine.curshape, engine.curx, engine.cury);
				  engine_move (&engine,ACTION_DROP);
				  fprintf(logfile, "Drop completed: final position (x=%d, y=%d)\n", 
				          engine.curx, engine.cury);
				  finished = evaluate(&engine);          /* prevent key press after drop */
				  break;
				  /* show next piece */
				case 's':
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s Show next piece enabled\n", timestamp_str);
				  shownext = TRUE;
				  break;
				  /* toggle dotted lines */
				case 'd':
				  dottedlines = !dottedlines;
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s Dotted lines toggled: %s\n", timestamp_str, 
				          dottedlines ? "ON" : "OFF");
				  break;
				  /* next level */
				case 'a':
				  if (level < MAXLEVEL)
					{
					   level++;
					   in_timeout (DELAY);
					   get_timestamp_string(timestamp_str, sizeof(timestamp_str));
					   fprintf(logfile, "%s Level increased to %d\n", timestamp_str, level);
					}
				  else out_beep ();
				  break;
				  /* quit */
				case 'q':
				  finished = TRUE;
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s Player quit game\n", timestamp_str);
				  break;
				  /* pause */
				case 'p':
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s Game paused\n", timestamp_str);
				  out_setcolor (COLOR_WHITE,COLOR_BLACK);
				  out_gotoxy ((out_width () - 34) / 2,out_height () - 2);
				  out_printf ("Paused - Press any key to continue");
				  while ((ch = in_getch ()) == ERR) ;	/* Wait for a key to be pressed */
				  in_flush ();							/* Clear keyboard buffer */
				  out_gotoxy ((out_width () - 34) / 2,out_height () - 2);
				  out_printf ("                                  ");
				  get_timestamp_string(timestamp_str, sizeof(timestamp_str));
				  fprintf(logfile, "%s Game resumed\n", timestamp_str);
				  break;
				  /* unknown keypress */
				default:
				  out_beep ();
			   }
			 in_flush ();
		  }
		else
		  finished = evaluate(&engine);
	 }
   while (!finished);
   /* Restore console settings and exit */
   io_close ();
   
   /* Log game end information */
   get_timestamp_string(timestamp_str, sizeof(timestamp_str));
   fprintf(logfile, "GAME FINISHED at timestamp = %s\n", timestamp_str);
   fprintf(logfile, "Final position: shape %d at (x=%d, y=%d)\n", 
           engine.curshape, engine.curx, engine.cury);
   if (ch == 'q') {
       fprintf(logfile, "Cause: Player quit\n");
   }
   
   /* Don't bother the player if he want's to quit */
   if (ch != 'q')
	 {
		showplayerstats (&engine);
		savescores (GETSCORE (engine.score));
	 }
   closelogfile();
   exit (EXIT_SUCCESS);
}

