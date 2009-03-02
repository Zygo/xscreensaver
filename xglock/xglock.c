/*-
 * xglock.c - main file for xglock, the GTK+ gui interface to xlock.
 *
 * Copyright (c) 1998 by An Unknown author & Remi Cohen-Scali
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * Jul-98: Continual improvements by Remi Cohen-Scali <remi.cohenscali@pobox.com> 
 * ?? -98: First written by Charles Vidal <vidalc@club-internet.fr>.
 */

/*-
  XgLock Problems are almost the same as XmLock's ones.
    1. Allowing only one in -inroot.  Need a way to kill it. (a menu ?)
    2. Still missing many options (TO DO).
    3. Need a clean rewritting.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
# define XGLOCK_DEBUG    1
#else
# undef XGLOCK_DEBUG
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# define HAVE_UNISTD_H          1
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_VFORK
#define FORK                            vfork
#define FORK_STR                        "vfork"
#else
#define FORK                            fork
#define FORK_STR                        "fork"
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#ifdef HAVE_GTK_FONTSEL_WIDGET
#include <gtk/gtkfontsel.h>
#endif

#include "lmode.h"

/* Alloc args by chunks of */
#define ARGS_CHUNK                      20
#define MODE_STRING                     "-mode"

#define XLOCK                           "xlock"
#define WINDOW_WIDTH                    160
#define WINDOW_HEIGHT                   100
#define WINDOW_GEOMETRY                 "160x100"

#define XGLOCK_SPACING                  3
#define XGLOCK_TABLE_HOMOGENEOUS        ARG_HOMOGENEOUS
#define XGLOCK_TABLE_SPACING            XGLOCK_SPACING
#define XGLOCK_TABLE_ROW_SPACING        XGLOCK_SPACING
#define XGLOCK_TABLE_COL_SPACING        XGLOCK_SPACING
#define XGLOCK_TABLE_BORDER_WIDTH       XGLOCK_SPACING

#define FRAME_WIDTH                     500
#define FRAME_HEIGHT                    300

#define FONT_DIALOG_WIDTH               600
#define FONT_DIALOG_HEIGHT              400

#define MAX_WIDGET_NUMBER_LEN           25

static char numberwidget[MAX_WIDGET_NUMBER_LEN];
static pid_t numberprocess = -1;
static pid_t *launchedprocesses =(pid_t *)NULL;
static int num_launchedprocesses = 0;
static int sz_launchedprocesses = 0;
static int selected_mode = -1;
#ifndef HAVE_GTK_FONTSEL_WIDGET
static GtkWidget *font_sel_entry =(GtkWidget *)NULL;
#endif

/* Prototypes */
#if defined( __ANSI_C__ ) || defined( NeedFunctionsPrototypes )
# define __PROTO( name, args )  name args
#else /* ! __ANSI_C__ && ! NeedFunctionsPrototypes */
# define __PROTO( name, args )  name ()
#endif /* ? __ANSI_C__ && ! NeedFunctionsPrototypes */

#define STD_CB_ARGS	(GtkWidget *, gpointer)

static void *__PROTO( secured_malloc, (size_t) ); 
static void *__PROTO( secured_calloc, (int, size_t) ); 
static void *__PROTO( secured_realloc, (void *, size_t) ); 
static gchar *__PROTO( check_quotes, (gchar *str) ); 
    
static void __PROTO( mode_list_item_select_cb, STD_CB_ARGS );
static void __PROTO( mode_list_item_unselect_cb, STD_CB_ARGS );
static void __PROTO( compose_xlock_command, (char *) ); 
static void __PROTO( kill_xlock_cb, STD_CB_ARGS );
static void __PROTO( launch_xlock, STD_CB_ARGS ); 
static void __PROTO( exit_xglock, STD_CB_ARGS ); 
static void __PROTO( bool_option_clicked_cb, STD_CB_ARGS );
static void __PROTO( destroy_window, STD_CB_ARGS ); 

static void __PROTO( color_selection_cancel_cb, STD_CB_ARGS );
static void __PROTO( color_selection_ok_cb, STD_CB_ARGS ); 
static void __PROTO( color_selection_changed_cb, STD_CB_ARGS );
static void __PROTO( create_color_selection_dialog, STD_CB_ARGS ); 

#ifndef HAVE_GTK_FONTSEL_WIDGET
static void __PROTO( font_select_item_select_cb, STD_CB_ARGS );
#endif
static void __PROTO( font_select_cancel_cb, STD_CB_ARGS ); 
static void __PROTO( font_select_ok_cb, STD_CB_ARGS ); 
static void __PROTO( create_font_selection_dialog, STD_CB_ARGS ); 

static void __PROTO( file_selection_cancel_cb, STD_CB_ARGS ); 
static void __PROTO( file_selection_ok_cb, STD_CB_ARGS ); 
static void __PROTO( create_file_selection_dialog, STD_CB_ARGS ); 

static void __PROTO( contextual_help_dialog, STD_CB_ARGS ); 
static void __PROTO( window_help_dialog, STD_CB_ARGS ); 
static void __PROTO( global_help_dialog, STD_CB_ARGS ); 
static void __PROTO( about_dialog, STD_CB_ARGS ); 

static GtkWidget * __PROTO( create_file_menu, (GtkWidget *) ); 
static GtkWidget * __PROTO( create_help_menu, (GtkWidget *) ); 

static void __PROTO( create_fntColorOptions_entries, (GtkWidget *) ); 
static void __PROTO( create_genOptions_entries, (GtkWidget *) ); 
static void __PROTO( create_boolOptions_buttons, (GtkWidget *) ); 

/*
 * secured_malloc
 * --------------
 * Return an allocated string of bytes
 */
static gpointer
secured_malloc(gulong sz)
{
    gpointer allocated =(gpointer)NULL;
    allocated =(gpointer)g_malloc( sz ); 
    g_assert( allocated ); 
    return allocated;
}

/*
 * secured_calloc
 * --------------
 * Return an allocated string of bytes
 */
static gpointer
secured_calloc(gulong n,
               gulong sz)
{
    gpointer allocated = NULL;

    allocated =(gpointer )g_malloc( n * sz );
    g_assert( allocated ); 
    return allocated; 
}

/*
 * secured_realloc
 * ---------------
 * Return an allocated string of bytes
 */
static gpointer 
secured_realloc(gpointer ptr,
                gulong sz)
{
    gpointer allocated = NULL;

    allocated =(gpointer )g_realloc( ptr, sz );
    g_assert( allocated ); 
    return allocated;
}

/*
 * compose_xlock_command
 * ---------------------
 * This function scan the string passed as argument to see if
 * it is necessary to suround it with quotes for using it as a command
 * line argument.
 */
static gchar *
check_quotes(gchar *str)
{
    static gchar *req_dbl = "'#&;* "; 
    static gchar *req_simple = "\"";
    gint len = strlen( str );
    gint dbl_cspn = strcspn( str, req_dbl ); 
    gint simple_cspn = strcspn( str, req_simple );
    gchar *quotes = ""; 

    if ( dbl_cspn != len )
      quotes = "\"";
    if ( simple_cspn != len )
      quotes = "'";
    return quotes; 
}

/*
 * compose_xlock_command
 * ---------------------
 * This function scan all the selected options and build an according
 * command line for running xlock.
 */
static void
compose_xlock_command(gchar ***cmd_argv_p,
                      gulong  *cmd_argv_sz,
                      gulong  *cmd_argc)
{
    gint        i;
    gulong      n = *cmd_argc;
    gulong      sz = *cmd_argv_sz;
    gchar     **cmd_argv = *cmd_argv_p; 

#ifdef XGLOCK_DEBUG
# define CHECK_REALLOC(num, size, need)                                                 \
    fprintf(stderr,                                                                     \
            "check realloc: current=%ld need=%d size=%ld\n",                            \
            num, need, size );                                                          \
    if (size <= num+need) {                                                             \
        fprintf(stderr,                                                                 \
                "must realloc: size <= num+need is %s\n",                               \
                size<=num+need?"TRUE":"FALSE");                                         \
        cmd_argv =(char **)secured_realloc((gpointer)cmd_argv,                          \
                                           (gulong)(size+ARGS_CHUNK)*sizeof(char **));  \
        size+= ARGS_CHUNK;                                                              \
    }

#else /* ! XGLOCK_DEBUG */
# define CHECK_REALLOC(num, size, need)                                                 \
    if (size <= num+need) {                                                             \
        cmd_argv =(char **)secured_realloc((gpointer)cmd_argv,                          \
                                           (gulong)(size+ARGS_CHUNK)*sizeof(char **));  \
        size+= ARGS_CHUNK;                                                              \
    }

#endif /* ? XGLOCK_DEBUG */

    for (i = 0; i < nb_boolOpt; i++)
      if (BoolOpt[i].value) {

          /* Check dynamic tab size */
          CHECK_REALLOC( n, sz, 1 );

          /* Add the arg */
          cmd_argv[n] = secured_malloc( strlen( BoolOpt[i].cmdarg )+2 );
          (void) sprintf( cmd_argv[n], "-%s", BoolOpt[i].cmdarg );
          n++; 
      }
    
    /* General options */
    for (i = 0; i < nb_genOpt; i++)
    {
        gchar *val;
        
        if ( !( GTK_IS_ENTRY( generalOpt[i].text_widget ) ) ) continue; 
        
        val = gtk_entry_get_text( GTK_ENTRY( generalOpt[i].text_widget ));
        if ( strlen( val ) > 0 )
        {
            gint j;
            gchar *quotes; 

            for ( j = 0; j < nb_defaultedOptions; j++ )
              if ( !strcmp( generalOpt[i].cmdarg, defaulted_options[j] ) )
                break;
            if ( j < nb_defaultedOptions )
            {
                gint defval = 0;
                
                switch ( j )
                {
                  case DEF_DELAY:
                    defval = LockProcs[selected_mode].def_delay; break; 
                  case DEF_BATCHCOUNT: 
                    defval = LockProcs[selected_mode].def_batchcount; break; 
                  case DEF_CYCLES: 
                    defval = LockProcs[selected_mode].def_cycles; break; 
                  case DEF_SIZE: 
                    defval = LockProcs[selected_mode].def_size; break; 
                  case DEF_SATURATION:
                    defval = LockProcs[selected_mode].def_saturation; break; 
                }
                
                if ( defval == atoi( val ) )
#ifdef XGLOCK_DEBUG
                {
                    fprintf(stderr,
                            "Option %s has its default value (%d): discarded\n",
                            generalOpt[i].cmdarg, defval);
#endif
                    continue;
#ifdef XGLOCK_DEBUG
                }
#endif
            }
                          
            /* Check dyntab size */
            CHECK_REALLOC( n, sz, 2 );

            cmd_argv[n] = secured_malloc( strlen( generalOpt[i].cmdarg )+2 );
            (void) sprintf( cmd_argv[n], "-%s", generalOpt[i].cmdarg );
            n++;

            quotes = check_quotes( val );
            
            cmd_argv[n] = secured_malloc( strlen( val )+3 );
            (void) sprintf( cmd_argv[n], "%s%s%s", quotes, val, quotes);
            n++;
        }
    }
    
    /* font and color options */
    for (i = 0; i < nb_fntColorOpt; i++)
    {
        switch ( fntcolorOpt[i].type )
        {
            /* Handle color parameter */
          case TFNTCOL_COLOR:
          {
              gchar *val = gtk_entry_get_text( GTK_ENTRY( fntcolorOpt[i].entry ) );
              gushort R, G, B;
              gdouble T;
              gchar   opt[50]; 
              
              if ( strlen( val ) > 0 )
              {
                  CHECK_REALLOC( n, sz, 2 );

                  cmd_argv[n] = secured_malloc( strlen( fntcolorOpt[i].cmdarg )+2 );
                  (void) sprintf( cmd_argv[n], "-%s", fntcolorOpt[i].cmdarg );
                  n++;

                  sscanf( val, "#%4hx%4hx%4hx (%lf)", &R, &G, &B, &T );
                  sprintf( opt, "#%04x%04x%04x", R, G, B );

                  cmd_argv[n] = secured_malloc( strlen( opt )+3 );
                  (void) sprintf( cmd_argv[n], "\"%s\"", opt );
                  n++;
              }
              break;
          }

            /* Handle font or file parameters */
          case TFNTCOL_FONT:
          case TFNTCOL_FILE:
          {
              gchar *val = gtk_entry_get_text( GTK_ENTRY( fntcolorOpt[i].entry ) );
              gchar *quotes; 
              
              if (strlen( val ) > 0)
              {
                  CHECK_REALLOC( n, sz, 2 );

                  cmd_argv[n] = secured_malloc( strlen( fntcolorOpt[i].cmdarg )+2 );
                  (void) sprintf( cmd_argv[n], "-%s", fntcolorOpt[i].cmdarg );
                  n++;

                  quotes = check_quotes( val ); 

                  cmd_argv[n] = secured_malloc( strlen( val )+3 );
                  (void) sprintf( cmd_argv[n], "%s%s%s", quotes, val, quotes);
                  n++;
              }
              break; 
          }
        }
    }

    /* Add a NULL ptr */
    CHECK_REALLOC( n, sz, 1 );
    cmd_argv[n++] =(char *)NULL;

#undef CHECK_REALLOC

    /* Return argv, argc, argsz */
    *cmd_argc = n;
    *cmd_argv_sz = sz;
    *cmd_argv_p = cmd_argv; 
}

/*
 * kill_xlock_cb
 * -------------
 * Callback associated wih the Kill xlock file menu entry
 */
static void
kill_xlock_cb(GtkWidget *w,
              gpointer udata)
{
    guint i;
    gint dummy = 0;

    for ( i = 0;  i < num_launchedprocesses; i++ )
      if ( launchedprocesses[i] != -1 ) {
          (void) kill( launchedprocesses[i], SIGKILL );
          (void) wait( &dummy );
          launchedprocesses[i] = -1; 
      }
    num_launchedprocesses = 0; 
}

/*
 * launch_xlock
 * ------------
 * This callback launch the xlock command accordingly to the selected
 * modes and options. The xlock process is launched in root window, in
 * a window or in normal mode.
 */
static void 
launch_xlock(GtkWidget *w,
             gpointer udata )
{
#ifdef XGLOCK_DEBUG
    gint i; 
#endif
    gchar  *launch_opt =(char *)udata;
    gulong  args_size = 0,
            args_cnt = 0;
    gchar **cmd_argv =(char **)NULL;
    
    if ( selected_mode >= 0 && selected_mode < nb_mode )
    {
        /* Alloc argv table */
        args_size = ARGS_CHUNK;
        args_cnt = 0; 
        cmd_argv =(char **)secured_calloc( ARGS_CHUNK, sizeof(char *) );

        /* First arg is xlock itself */
        cmd_argv[args_cnt] = secured_malloc( strlen( XLOCK )+1 );
        (void) strcpy(cmd_argv[args_cnt], XLOCK);
        args_cnt++;

        /* If launch in window/root */
        if ( launch_opt )
        {
            cmd_argv[args_cnt] = secured_malloc( strlen( launch_opt )+1 );
            (void) strcpy(cmd_argv[args_cnt], launch_opt);
            args_cnt++;
        }

        /* Mode arg */
        cmd_argv[args_cnt] = secured_malloc( strlen( MODE_STRING )+1 );
        (void) strcpy(cmd_argv[args_cnt], MODE_STRING);
        args_cnt++; 

        /* Mode name */
        cmd_argv[args_cnt] = secured_malloc( strlen( LockProcs[selected_mode].cmdline_arg )+1 );
        (void) strcpy(cmd_argv[args_cnt], LockProcs[selected_mode].cmdline_arg);
        args_cnt++; 

        /* Add other user selected args */
        compose_xlock_command( &cmd_argv, &args_size, &args_cnt );
        
#ifdef XGLOCK_DEBUG
        fprintf( stderr, "Running command: " );
        for ( i = 0; i < args_cnt; i++ )
          fprintf( stderr, "%s ", cmd_argv[i] ? cmd_argv[i] : "\n" );
#endif
        /* Allocate the launched pid_t dyntab */
        if ( !launchedprocesses )
        {
            launchedprocesses = secured_calloc( 10, sizeof( pid_t ) );
            sz_launchedprocesses = 10;
        }
        /* Grow dyntab */
        if ( num_launchedprocesses == sz_launchedprocesses )
        {
            launchedprocesses = secured_realloc(launchedprocesses,
                                                (sz_launchedprocesses+10)*sizeof( pid_t ) );
            sz_launchedprocesses+= 10;
        }
        /* Fork process to run xlock */
        if ( (launchedprocesses[num_launchedprocesses] = FORK() ) == -1 )
        {
            perror( FORK_STR ); 
            fprintf(stderr,
                    "Cannot " FORK_STR " process for xlock !\n" );
        }
        /* Child process */
        else if ( launchedprocesses[num_launchedprocesses] == 0 )
          (void) execvp( XLOCK, cmd_argv );
        else
          num_launchedprocesses++; 
    }

    /* Free resources */
    while ( args_cnt-- )
      (void) free( cmd_argv[args_cnt] );
    (void) free( cmd_argv );
    cmd_argv =(char **)NULL; 
}

/*
 * exit_xglock
 * -----------
 * This callback terminates the xglock process. It kills the root
 * widget and break the gtk main loop.
 */
static void 
exit_xglock(GtkWidget *w,
            gpointer udata )
{
    GtkWidget *root =(GtkWidget *)udata; 
    
    gtk_main_quit();
    gtk_widget_destroy(root); 
}

/*
 * destroy_window
 * --------------
 * Callback for destroying window
 */
static void
destroy_window(GtkWidget * widget,
               gpointer udata)
{
    gtk_widget_destroy( (GtkWidget *)udata ); 
}

/*
 * bool_option_clicked_cb
 * ----------------------
 * This callback is called when a check button is clicked. It's
 * state is then saved in the structure.
 */
static void
bool_option_clicked_cb(GtkWidget *w,
                       gpointer udata )
{
    guint optnr =(guint)udata;

    if ( !( GTK_IS_CHECK_BUTTON(w) )) return; 
    
    BoolOpt[optnr].value = !BoolOpt[optnr].value;
#ifdef XGLOCK_DEBUG
    fprintf(stderr,
            "Option #%ud %s is %s\n",
            optnr, BoolOpt[optnr].cmdarg,
            BoolOpt[optnr].value ? "ACTIVE":"INACTIVE" );
#endif
}

/* ================================================================= */
/*                                                                   */
/*                      List items' callbacks                        */
/*                                                                   */
/* ================================================================= */

/*
 * mode_list_item_select_cb
 * ------------------------
 * Callback called when a mode list item is selected in the list.
 * It fork the process and exec xlock in the specified mode.
 */
static void 
mode_list_item_select_cb(GtkWidget *w,
                         gpointer mode_nr )
{
    gint mode =(gint)mode_nr;
    gint i; 

    if (w->state != GTK_STATE_SELECTED ) return; 

    if ( mode < 0 || mode >= nb_mode ) return; 

    selected_mode = mode;

    for ( i = 0; i < nb_defaultedOptions; i++ )
    {
        gchar temp[30];
        gint defval = 0, j;

        for ( j = 0; j < nb_mode; j++ )
          if ( !strcmp( generalOpt[j].cmdarg, defaulted_options[i] ) )
            break;
        if ( j == nb_mode ) break; 
        switch ( i ) {
          case DEF_DELAY:
            defval = LockProcs[selected_mode].def_delay; break; 
          case DEF_BATCHCOUNT:
            defval = LockProcs[selected_mode].def_batchcount; break; 
          case DEF_CYCLES:
            defval = LockProcs[selected_mode].def_cycles; break; 
          case DEF_SIZE:
            defval = LockProcs[selected_mode].def_size; break; 
          case DEF_SATURATION:
            defval = LockProcs[selected_mode].def_saturation; break;
        }
        sprintf(temp, "%d", defval );
        gtk_entry_set_text(GTK_ENTRY(generalOpt[j].text_widget), temp);
    }

    if ( ( numberprocess = FORK() ) == -1 )
      fprintf( stderr, "Fork error !\n" );
    else if ( numberprocess == 0 ) {
        (void) execlp(XLOCK, XLOCK, "-parent", numberwidget,
                      MODE_STRING, LockProcs[mode].cmdline_arg, "-geometry", WINDOW_GEOMETRY, "-delay", "100000",
                      "-nolock", "-inwindow", "+install", 0);
    }
}

/*
 * mode_list_item_unselect_cb
 * --------------------------
 * Callback called when a mode list item is deselected in the list.
 * It kills the child process forked to display preview in the
 * preview window.
 */
static void 
mode_list_item_unselect_cb(GtkWidget *w, 
                           gpointer udata )
{
    gint dummy = 0;

    if ( w->state == GTK_STATE_SELECTED ) return;

    selected_mode = -1; 
    
    if ( numberprocess != -1 ) {
        (void) kill( numberprocess, SIGKILL );
        (void) wait( &dummy );
        numberprocess = -1; 
    }
}

/* ================================================================= */
/*                                                                   */
/*                      Color selection dialog                       */
/*                                                                   */
/* ================================================================= */

/*
 * color_selection_cancel_cb
 * -------------------------
 * Cancel callback for color selection dialog
 */
static void
color_selection_cancel_cb(GtkWidget *w,
                          gpointer udata)
{
    struct_option_fntcol_callback *cbs_ptr =(struct_option_fntcol_callback *)udata; 
    gtk_widget_hide(cbs_ptr->fntcol_dialog); 
    gtk_widget_destroy(cbs_ptr->fntcol_dialog);
    (void) g_free(udata); 
}

/*
 * color_selection_ok_cb
 * ---------------------
 * Ok callback for color selection dialog
 */
static void
color_selection_ok_cb(GtkWidget *w,
                      gpointer udata)
{
    struct_option_fntcol_callback *cbs_ptr =(struct_option_fntcol_callback *)udata; 
    GtkColorSelection 		  *colorsel;
    GdkColor 	       		   gdkcol_bg; 
    GdkColor 	       		   gdkcol_fg; 
    gdouble            		   colors[4];
    gchar              		   colorstr[30]; 

    colorsel = GTK_COLOR_SELECTION( GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel );

    gtk_color_selection_get_color(colorsel, colors);
    gtk_color_selection_set_color(colorsel, colors);

    sprintf(colorstr, "#%04x%04x%04x (%3.2f)",
            (gushort)(colors[0] * 0xffff),
            (gushort)(colors[1] * 0xffff),
            (gushort)(colors[2] * 0xffff),
            colors[3]); 

    gdkcol_bg.red = (gushort)(colors[0] * 0xffff); 
    gdkcol_bg.green = (gushort)(colors[1] * 0xffff); 
    gdkcol_bg.blue = (gushort)(colors[2] * 0xffff); 
    gdk_color_alloc(gdkcolormap, &gdkcol_bg);
    
#ifdef XGLOCK_DEBUG
    fprintf(stderr,
            "Parsed bg color: color=#%04x%04x%04x pixel=%lx\n",
            gdkcol_bg.red, 
            gdkcol_bg.green, 
            gdkcol_bg.blue,
            gdkcol_bg.pixel);
#endif
    gdkcol_fg.red = (gushort)((1 -colors[0]) * 0xffff); 
    gdkcol_fg.green = (gushort)((1 -colors[1]) * 0xffff); 
    gdkcol_fg.blue = (gushort)((1 -colors[2]) * 0xffff); 
    gdk_color_alloc(gdkcolormap, &gdkcol_fg);

#ifdef XGLOCK_DEBUG
    fprintf(stderr,
            "Parsed fg color: color=#%04x%04x%04x pixel=%lx\n",
            gdkcol_fg.red, 
            gdkcol_fg.green, 
            gdkcol_fg.blue,
            gdkcol_fg.pixel);
#endif

    gdk_flush(); 
    gdk_window_set_background(GTK_WIDGET(cbs_ptr->drawing_area)->window, &gdkcol_bg);
    gtk_entry_set_text( GTK_ENTRY(cbs_ptr->entry), colorstr );
    gtk_widget_hide( cbs_ptr->fntcol_dialog ); 
    gtk_widget_destroy(cbs_ptr->fntcol_dialog);
/*     XFlush( GDK_WINDOW_XDISPLAY( GTK_WIDGET(cbs_ptr->drawing_area)->window ) ); */
/*     gdk_flush();  */
    (void) g_free(udata); 
}

/*
 * color_selection_changed_cb
 * --------------------------
 * Color changed callback for color selection dialog (update the current color)
 */
static void
color_selection_changed_cb(GtkWidget *w,
                           gpointer udata)
{
    GtkWidget		 *colorsel_dialog =(GtkWidget *)udata; 
    GtkColorSelection 	 *colorsel; 
    gdouble            	  colors[4];

    colorsel = GTK_COLOR_SELECTION( GTK_COLOR_SELECTION_DIALOG(colorsel_dialog)->colorsel );
    gtk_color_selection_get_color(colorsel, colors);
}

/*
 * create_color_selection_dialog
 * -----------------------------
 * Create a color selection dialog. Is the callback for a color option
 * choice button
 */
static void
create_color_selection_dialog(GtkWidget *w,
                              gpointer udata)
{
    struct_option_fntcol	  *optentry =(struct_option_fntcol *)udata; 
    gdouble    		  	   colors[4];
    gchar      		 	  *val; 
    struct_option_fntcol_callback *cbs_ptr; 

    cbs_ptr =(struct_option_fntcol_callback *)secured_malloc(sizeof(struct_option_fntcol_callback));
    
    gtk_preview_set_install_cmap(TRUE);
    gtk_widget_push_visual(gtk_preview_get_visual());
    gtk_widget_push_colormap(gtk_preview_get_cmap());

    cbs_ptr->fntcol_dialog = gtk_color_selection_dialog_new("color selection dialog");
    cbs_ptr->entry = optentry->entry; 
    cbs_ptr->drawing_area = optentry->drawing_area; 

    val = gtk_entry_get_text( GTK_ENTRY(cbs_ptr->entry) );
    if ( strlen( val ) > 0 )
    {
        gushort R, G, B;
        
        sscanf( val, "#%04hx%04hx%04hx (%lf)", &R, &G, &B, &colors[3]);
        colors[0] = R /(gdouble)0xffff;
        colors[1] = G /(gdouble)0xffff; 
        colors[2] = B /(gdouble)0xffff; 
        gtk_color_selection_set_opacity(GTK_COLOR_SELECTION( GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel),
                                        colors[3] < 1.0);
        gtk_color_selection_set_color(GTK_COLOR_SELECTION( GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel),
                                      colors);
    }

    gtk_color_selection_set_opacity(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel),
                                    TRUE);

    gtk_color_selection_set_update_policy(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel),
                                          GTK_UPDATE_CONTINUOUS);

    gtk_window_position(GTK_WINDOW(cbs_ptr->fntcol_dialog), GTK_WIN_POS_MOUSE);
    gtk_signal_connect(GTK_OBJECT(cbs_ptr->fntcol_dialog), "destroy",
                       (GtkSignalFunc) destroy_window,
                       (gpointer) cbs_ptr->fntcol_dialog);

    gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->colorsel),
                       "color_changed",
                       (GtkSignalFunc) color_selection_changed_cb,
                       (gpointer) cbs_ptr->fntcol_dialog);

    gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->ok_button),
                       "clicked",
                       (GtkSignalFunc) color_selection_ok_cb,
                       (gpointer) cbs_ptr);
    
    gtk_signal_connect_object(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->cancel_button),
                              "clicked",
                              (GtkSignalFunc) color_selection_cancel_cb,
                              (gpointer) cbs_ptr);

    gtk_widget_pop_colormap();
    gtk_widget_pop_visual();
    gtk_widget_show(cbs_ptr->fntcol_dialog);
}

/* ================================================================= */
/*                                                                   */
/*                       Font selection dialog                       */
/*                                                                   */
/* ================================================================= */

/*
 * This code is used when xglock is compiled with the marvelous Gtk
 * Font Selection Widget written by Damon Chaplin. The problem is that
 * this widget is GPL'ed and xlockmore is BSD licensed. This widget cannot
 * be embedded in this code. In order to use it, build a libgtkfontsel.a,
 * library, install it in /usr/local/lib. Install gtkfontsel.h in
 * /usr/local/include.
 * Then use the --with-gtk-fontsel-prefix=/usr/local to enable this code.
 * If you do not have this widget, you'll have a list of font names.
 */
#ifdef HAVE_GTK_FONTSEL_WIDGET

/*
 * font_select_cancel_cb
 * ---------------------
 * Font dialog cancel button callback
 */
static void
font_select_cancel_cb(GtkWidget *w,
                      gpointer udata)
{
    struct_option_fntcol_callback *cbs_ptr = (struct_option_fntcol_callback *)udata; 
    gtk_widget_hide(cbs_ptr->fntcol_dialog); 
    gtk_widget_destroy(cbs_ptr->fntcol_dialog); 
    (void) g_free(udata); 
}

/*
 * font_select_ok_cb
 * -----------------
 * Font dialog ok button callback
 */
static void
font_select_ok_cb(GtkWidget *w,
                  gpointer udata)
{
    struct_option_fntcol_callback *cbs_ptr = (struct_option_fntcol_callback *)udata; 
    gtk_widget_hide(cbs_ptr->fntcol_dialog); 
    gtk_entry_set_text(GTK_ENTRY(cbs_ptr->entry),
                       gtk_font_selection_get_font_name(GTK_FONT_SELECTION(cbs_ptr->fntcol_dialog))); 
    gtk_widget_destroy(cbs_ptr->fntcol_dialog);
    (void) g_free(udata); 
}

/*
 * create_font_selection_dialog
 * ----------------------------
 * Create a font selection dialog for font parameters 
 */
static void
create_font_selection_dialog(GtkWidget *w,
                             gpointer udata)
{
    struct_option_fntcol 	  *optentry =(struct_option_fntcol *)udata;
    struct_option_fntcol_callback *cbs_ptr; 
    gchar 			  *fontname; 

    cbs_ptr =(struct_option_fntcol_callback *)secured_malloc( sizeof(struct_option_fntcol_callback) );
    cbs_ptr->fntcol_dialog =(GtkWidget *)NULL; 
    cbs_ptr->entry = optentry->entry; 
    cbs_ptr->drawing_area =(GtkWidget *)NULL; 

    cbs_ptr->fntcol_dialog = gtk_font_selection_new();
    fontname = gtk_entry_get_text(GTK_ENTRY(optentry->entry));
    if ( fontname && strlen(fontname) )
      gtk_font_selection_set_font_name(GTK_FONT_SELECTION(cbs_ptr->fntcol_dialog), fontname); 
    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->ok_button), "clicked",
                       GTK_SIGNAL_FUNC(font_select_ok_cb),
                       (gpointer) cbs_ptr);
    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(cbs_ptr->fntcol_dialog)->cancel_button), "clicked",
                       GTK_SIGNAL_FUNC(font_select_cancel_cb),
                       (gpointer) cbs_ptr);
    gtk_widget_show(cbs_ptr->fntcol_dialog);
}

#else /* ! HAVE_GTK_FONTSEL_WIDGET */

/*
 * font_select_item_select_cb
 * --------------------------
 * Font dialog callback called when a font item is selected in
 * order to update dialog entry
 */
void
font_select_item_select_cb(GtkWidget *w,
                           gpointer udata)
{
    gchar     			      **fontnames;
    gint 				num_fonts;
    gint 				font_nr =(gint)udata;

    fontnames = XListFonts (gdk_display, "-*-*-*-*-*-*-0-0-75-75-*-0-*-*", 32767, &num_fonts);
    gtk_entry_set_text(GTK_ENTRY(font_sel_entry), fontnames[font_nr]); 
}

/*
 * font_select_cancel_cb
 * ---------------------
 * Font selection dialog cancel callback
 */
void
font_select_cancel_cb(GtkWidget *w,
                      gpointer udata)
{
    struct_option_fntcol_callback      *cbs_ptr =(struct_option_fntcol_callback *)udata;

    gtk_widget_hide( cbs_ptr->fntcol_dialog ); 
    gtk_widget_destroy( cbs_ptr->fntcol_dialog );
    (void)g_free( udata ); 
}

/*
 * font_select_ok_cb
 * -----------------
 * Font selection dialog ok callback
 */
void
font_select_ok_cb(GtkWidget *w,
		  gpointer udata)
{
    struct_option_fntcol_callback      *cbs_ptr =(struct_option_fntcol_callback *)udata;

    gtk_widget_hide( cbs_ptr->fntcol_dialog ); 
    gtk_entry_set_text(GTK_ENTRY(cbs_ptr->entry), gtk_entry_get_text(GTK_ENTRY(font_sel_entry))); 
    gtk_widget_destroy( cbs_ptr->fntcol_dialog ); 
    (void)g_free( udata ); 
}

/*
 * create_font_selection_dialog
 * ----------------------------
 * Create a font selection dialog for font parameters 
 */
void
create_font_selection_dialog(GtkWidget *w,
                             gpointer udata)
{
    struct_option_fntcol 	  *optentry =(struct_option_fntcol *)udata;
    struct_option_fntcol_callback *cbs_ptr; 
    GtkWidget 			  *window;
    GtkTooltips 		  *tooltips;
    GtkWidget 			  *box1;
    GtkWidget 			  *box2;
    GtkWidget 			  *scrolled_win;
    GtkWidget 			  *list;
    GtkWidget 			  *list_item;
    GtkWidget 			  *button;
    gchar                        **fontnames;
    gint 	                   num_fonts,
    		                   i;
    gchar 	                   temp[255];
    gchar 	                  *val;
    gint 	                   ftfound = -1; 

    cbs_ptr =(struct_option_fntcol_callback *)secured_malloc( sizeof(struct_option_fntcol_callback) );
    cbs_ptr->fntcol_dialog =(GtkWidget *)NULL;
    cbs_ptr->entry = optentry->entry; 
    cbs_ptr->drawing_area =(GtkWidget *)NULL; 
    font_sel_entry =(GtkWidget *)NULL; 

    cbs_ptr->fntcol_dialog = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(window, "main window");
    gtk_window_set_title(GTK_WINDOW(window), "xlock");
    
    tooltips = gtk_tooltips_new();
    
    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), box1);
    gtk_widget_show(box1);
    
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_usize(scrolled_win, 600, 400 );
    gtk_container_border_width(GTK_CONTAINER(scrolled_win), 1);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(box1), scrolled_win);
    gtk_widget_show(scrolled_win);
    
    list = gtk_list_new ();
    gtk_list_set_selection_mode(GTK_LIST(list), GTK_SELECTION_BROWSE);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), list);
    gtk_container_set_focus_vadjustment (GTK_CONTAINER (list),
                                         gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_win)));
    GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (scrolled_win)->vscrollbar, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, list, "Select the font you want",(gchar *)NULL);

    val = gtk_entry_get_text( GTK_ENTRY(cbs_ptr->entry) );
    
    fontnames = XListFonts (gdk_display, "-*-*-*-*-*-*-0-0-75-75-*-0-*-*", 32767, &num_fonts);
    for (i = 0; i < num_fonts; i++)
      if ( !strcmp( val, fontnames[i] ) )
      {
          ftfound = i;
          break; 
      }
    for (i = 0; i < num_fonts; i++) {
        sprintf(temp, "%s", fontnames[i]);
        list_item = gtk_list_item_new_with_label(temp);
        gtk_container_add(GTK_CONTAINER(list), list_item);
        gtk_signal_connect (GTK_OBJECT (list_item), "select",
                            (GtkSignalFunc) font_select_item_select_cb, 
                            (gpointer) i);
        gtk_widget_show(list_item);
    }
    gtk_widget_show( list );
    
    font_sel_entry = gtk_entry_new();
    if ( ftfound >= 0 )
      gtk_entry_set_text( GTK_ENTRY(font_sel_entry), val );
    gtk_tooltips_set_tip(tooltips, font_sel_entry, "Selected font",(gchar *)NULL);
    gtk_box_pack_start(GTK_BOX(box1), font_sel_entry, FALSE, TRUE, 0);
    gtk_widget_show(font_sel_entry);
    
    if ( ftfound >= 0 )
    {
        gtk_list_unselect_item( GTK_LIST(list), 0 ); 
        gtk_list_select_item( GTK_LIST(list), ftfound );
    }

    /* HBox for buttons */
    box2 = gtk_hbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(box2), 10);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 5);
    gtk_widget_show(box2);
        
    /* button OK */
    button = gtk_button_new_with_label("Ok");
    gtk_widget_set_usize(button, 50, 20 );
    gtk_container_border_width(GTK_CONTAINER(button), 1);
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, button, "Accept selected font",(gchar *)NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) font_select_ok_cb,
                       (gpointer) cbs_ptr);
    gtk_box_pack_start(GTK_BOX(box2), button, TRUE, FALSE, 5);
    gtk_widget_show(button);
    
    /* button Cancel */
    button = gtk_button_new_with_label("Cancel");
    gtk_widget_set_usize(button, 50, 20 );
    gtk_container_border_width(GTK_CONTAINER(button), 1);
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, button, "Accept selected font",(gchar *)NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) font_select_cancel_cb,
                       (gpointer) cbs_ptr);
    gtk_box_pack_start(GTK_BOX(box2), button, TRUE, FALSE, 5);
    gtk_widget_show(button);

    gtk_widget_show(window); 
}

#endif /* ? HAVE_GTK_FONTSEL_WIDGET */

/* ================================================================= */
/*                                                                   */
/*                       File selection dialog                       */
/*                                                                   */
/* ================================================================= */

/*
 * file_selection_cancel_cb
 * ------------------------
 * File selector Cancel callback
 */
static void
file_selection_cancel_cb(GtkWidget *w,
                         gpointer udata)
{
    struct_option_fntcol_callback *cbs_ptr= (struct_option_fntcol_callback *)udata; 
    gtk_widget_hide( cbs_ptr->fntcol_dialog );
    gtk_widget_destroy( cbs_ptr->fntcol_dialog );
    (void) g_free(udata);
}

/*
 * file_selection_ok_cb
 * --------------------
 * File selector OK callback
 */
static void
file_selection_ok_cb(GtkWidget *w,
                     gpointer udata)
{
    struct stat  		   stbuf; 
    struct_option_fntcol_callback *cbs_ptr= (struct_option_fntcol_callback *)udata; 
    gchar       		  *filename; 

    /* Be sure file exist. We could be more paranoia */
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(cbs_ptr->fntcol_dialog));
    if ( stat(filename, &stbuf) != -1 )
    {
        gtk_widget_hide(cbs_ptr->fntcol_dialog);
        gtk_entry_set_text(GTK_ENTRY(cbs_ptr->entry), filename);
        gtk_widget_destroy(cbs_ptr->fntcol_dialog); 
        (void) g_free(udata);
    }
}

/*
 * create_file_selection_dialog
 * ----------------------------
 * Create a file selection dialog for file path parameters ( 
 */
static void
create_file_selection_dialog(GtkWidget *w,
                             struct_option_fntcol *optentry )
{
    struct_option_fntcol_callback *cbs_ptr;
    
    cbs_ptr =(struct_option_fntcol_callback *)secured_malloc( sizeof(struct_option_fntcol_callback) );
    cbs_ptr->fntcol_dialog = gtk_file_selection_new("file selection dialog");
    cbs_ptr->entry = optentry->entry; 
    cbs_ptr->drawing_area =(GtkWidget *)NULL; 
    gtk_window_position(GTK_WINDOW(cbs_ptr->fntcol_dialog), GTK_WIN_POS_MOUSE);
    gtk_signal_connect(GTK_OBJECT(cbs_ptr->fntcol_dialog), "destroy",
                       (GtkSignalFunc) destroy_window,
                       (gpointer) cbs_ptr->fntcol_dialog);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(cbs_ptr->fntcol_dialog)->ok_button),
                       "clicked", (GtkSignalFunc) file_selection_ok_cb,
                       (gpointer) cbs_ptr);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(cbs_ptr->fntcol_dialog)->cancel_button),
                              "clicked", (GtkSignalFunc) file_selection_cancel_cb,
                              (gpointer) cbs_ptr);
    gtk_widget_show(cbs_ptr->fntcol_dialog);
}

/* ================================================================= */
/*                                                                   */
/*                        Help dialogs routines                      */
/*                                                                   */
/* ================================================================= */

static void
contextual_help_dialog(GtkWidget *w,
                       gpointer udata )
{
    fprintf(stderr, "contextual_help_dialog !\n"); 
}

static void
window_help_dialog(GtkWidget *w,
                   gpointer udata )
{
    fprintf(stderr, "window_help_dialog !\n"); 
}

static void
global_help_dialog(GtkWidget *w,
                   gpointer udata )
{
    fprintf(stderr, "global_help_dialog !\n"); 
}

static void
about_dialog(GtkWidget *w,
             gpointer udata )
{
    fprintf(stderr, "about_dialog !\n"); 
}


/* ================================================================= */
/*                                                                   */
/*                     Menu bar creation routines                    */
/*                                                                   */
/* ================================================================= */

/*
 * create_file_menu
 * ----------------
 * Create the file menu attached to file menubar item
 */
static GtkWidget *
create_file_menu(GtkWidget *window)
{
    GtkWidget           *menu;
    GtkWidget           *menu_item;
    GtkAccelGroup       *group;

    menu = gtk_menu_new();
    group = gtk_accel_group_new();
    gtk_menu_set_accel_group (GTK_MENU (menu), group);

    menu_item = gtk_menu_item_new_with_label("Kill xlock");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
			       "activate",
			       group,
			       'K',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) kill_xlock_cb,
                        (gpointer) window);
    gtk_widget_show(menu_item);
    
    menu_item = gtk_menu_item_new_with_label("Quit");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
			       "activate",
			       group,
			       'Q',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) exit_xglock,
                        (gpointer) window);
    gtk_widget_show(menu_item);

    return menu;
}

/*
 * create_help_menu
 * ----------------
 * Create the help menu attached to help menubar item
 */
static GtkWidget  *
create_help_menu(GtkWidget *window)
{
    GtkWidget           *menu;
    GtkWidget           *menu_item;
    GtkAccelGroup *group;

    menu = gtk_menu_new();
    group = gtk_accel_group_new();
    gtk_menu_set_accel_group (GTK_MENU (menu), group);

    menu_item = gtk_menu_item_new_with_label("Context help");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
                                   "activate",
			       group,
                                   'C',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) contextual_help_dialog,
                        (gpointer) window);
    gtk_widget_show(menu_item);
    
    menu_item = gtk_menu_item_new_with_label("Window help");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
                                   "activate",
			       group,
                                   'W',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) window_help_dialog,
                        (gpointer) window);
    gtk_widget_show(menu_item);
    
    menu_item = gtk_menu_item_new_with_label("Global help");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
                                   "activate",
			       group,
                                   'G',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) global_help_dialog,
                        (gpointer) window);
    gtk_widget_show(menu_item);
    
    menu_item = gtk_menu_item_new_with_label("About XgLock");
    gtk_container_add( GTK_CONTAINER (menu), menu_item);
    gtk_widget_add_accelerator(menu_item,
                                   "activate",
			       group,
                                   'A',
			       0, GTK_ACCEL_VISIBLE);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                        (GtkSignalFunc) about_dialog,
                        (gpointer) window);
    gtk_widget_show(menu_item);
    
    return menu;
}

/* ================================================================= */
/*                                                                   */
/*             Special parameters' dialog creation                   */
/*                                                                   */
/* ================================================================= */

/*
 * create_fntColorOptions_entries
 * ------------------------------
 * Create all entries for font and color options.
 */
static void
create_fntColorOptions_entries(GtkScrolledWindow *window)
{
    gint         i;
    GtkWidget   *box0;
    GtkWidget   *box1;
    GtkWidget   *table; 
    GtkWidget   *label; 
    GtkWidget   *entry;
    GtkWidget   *button =(GtkWidget *)NULL;
    GtkTooltips *tooltips;
    GdkColor	 gdkcol_bg; 
    GdkColor	 gdkcol_fg; 

    tooltips = gtk_tooltips_new();

    box0 = gtk_hbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(window, box0);
    gtk_widget_show(box0);

    table = gtk_table_new(nb_fntColorOpt,6,0);
    gtk_table_set_row_spacings(GTK_TABLE(table),XGLOCK_TABLE_ROW_SPACING);
    gtk_table_set_col_spacings(GTK_TABLE(table),XGLOCK_TABLE_COL_SPACING);
    gtk_container_border_width(GTK_CONTAINER(table),XGLOCK_TABLE_BORDER_WIDTH);
    gtk_box_pack_start(GTK_BOX(box0),table,TRUE,TRUE,XGLOCK_SPACING);
    gtk_widget_show(table);

    for (i = 0; i < nb_fntColorOpt; i++) {
          
        label = gtk_label_new(fntcolorOpt[i].label);
        gtk_table_attach(GTK_TABLE(table),label,
                         0,1,i,i+1,
                         GTK_EXPAND|GTK_FILL,
                         GTK_EXPAND|GTK_FILL,
                         XGLOCK_TABLE_SPACING,
                         XGLOCK_TABLE_SPACING);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT); 
        gtk_widget_show(label);
        
        fntcolorOpt[i].entry = entry = gtk_entry_new();
        gtk_entry_set_editable(GTK_ENTRY(entry), False); 
        gtk_widget_set_usize(entry, 0, 21); 
        gtk_tooltips_set_tip(tooltips, entry, fntcolorOpt[i].desc,(gchar *)NULL);
        gtk_table_attach(GTK_TABLE(table),entry,
                         1,5,i,i+1,
                         GTK_EXPAND|GTK_FILL,
                         GTK_EXPAND|GTK_FILL,
                         XGLOCK_TABLE_SPACING,
                         XGLOCK_TABLE_SPACING);
        gtk_widget_show(entry);

        switch ( fntcolorOpt[i].type ) {
            
          case TFNTCOL_COLOR:
            box1 = gtk_hbox_new(FALSE, 0);
            gtk_container_border_width(GTK_CONTAINER(box1),XGLOCK_TABLE_BORDER_WIDTH);
            gtk_table_attach(GTK_TABLE(table),box1,
                             5,6,i,i+1,
                             GTK_EXPAND|GTK_FILL,
                             GTK_EXPAND|GTK_FILL,
                             XGLOCK_TABLE_SPACING,
                             XGLOCK_TABLE_SPACING);      
            gtk_widget_show(box1);

            gdkcol_bg.red = 0x0000; 
            gdkcol_bg.green = 0x0000; 
            gdkcol_bg.blue = 0x0000; 
            gdk_color_alloc(gdkcolormap, &gdkcol_bg);
            gdkcol_bg.red = 0xffff; 
            gdkcol_bg.green = 0xffff; 
            gdkcol_bg.blue = 0xffff; 
            gdk_color_alloc(gdkcolormap, &gdkcol_fg);
            tooltips = gtk_tooltips_new();
            button = gtk_button_new_with_label( "Select color ..." ); 
            gtk_signal_connect(GTK_OBJECT(button), "clicked",
                               (GtkSignalFunc) create_color_selection_dialog,
                               (gpointer)&fntcolorOpt[i]);
            GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
            gtk_tooltips_set_tip(tooltips, button, fntcolorOpt[i].desc,(gchar *)NULL);
            gtk_box_pack_start(GTK_BOX(box1), button, FALSE, TRUE, 0); 
            gtk_widget_show(button);

            fntcolorOpt[i].drawing_area = gtk_drawing_area_new();
            gtk_tooltips_set_tip(tooltips, fntcolorOpt[i].drawing_area, fntcolorOpt[i].desc,(gchar *)NULL);
            gtk_widget_set_usize(fntcolorOpt[i].drawing_area, 10, 21);
            gtk_container_add(GTK_CONTAINER(box1), fntcolorOpt[i].drawing_area);
            gtk_widget_show(fntcolorOpt[i].drawing_area);

            break; 
            
          case TFNTCOL_FONT:
            button = gtk_button_new_with_label( "Select font ..." ); 
            gtk_signal_connect(GTK_OBJECT(button), "clicked",
                               (GtkSignalFunc) create_font_selection_dialog,
                               (gpointer)&fntcolorOpt[i]);
            GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
            gtk_tooltips_set_tip(tooltips, button, fntcolorOpt[i].desc,(gchar *)NULL);
            gtk_table_attach(GTK_TABLE(table),button,
                             5,6,i,i+1,
                             GTK_EXPAND|GTK_FILL,
                             GTK_EXPAND|GTK_FILL,
                             XGLOCK_TABLE_SPACING,
                             XGLOCK_TABLE_SPACING);      
            gtk_widget_show(button);
            break; 
            
          case TFNTCOL_FILE:
            button = gtk_button_new_with_label( "Select file ..." ); 
            gtk_signal_connect(GTK_OBJECT(button), "clicked",
                               (GtkSignalFunc) create_file_selection_dialog,
                               (gpointer)&fntcolorOpt[i]);
            GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
            gtk_tooltips_set_tip(tooltips, button, fntcolorOpt[i].desc,(gchar *)NULL);
            gtk_table_attach(GTK_TABLE(table),button,
                             5,6,i,i+1,
                             GTK_EXPAND|GTK_FILL,
                             GTK_EXPAND|GTK_FILL,
                             XGLOCK_TABLE_SPACING,
                             XGLOCK_TABLE_SPACING);      
            gtk_widget_show(button);
            break;

          default:
            fprintf(stderr,
                    "Program Inconsistency: Unknown parameter type #%d !\n",
                    fntcolorOpt[i].type ); 
        }

        if ( fntcolorOpt[i].drawing_area && GTK_WIDGET(fntcolorOpt[i].drawing_area)->window )
          gdk_window_set_background(GTK_WIDGET(fntcolorOpt[i].drawing_area)->window,
                                    &gdkcol_bg); 
    }
}

/* ================================================================= */
/*                                                                   */
/*                   Create general optrions entries                 */
/*                                                                   */
/* ================================================================= */

/*
 * create_genOptions_entries
 * -------------------------
 * Create all entries for general options (text fields).
 */
static void
create_genOptions_entries(GtkScrolledWindow *window)
{
    gint         i;
/*     gint         j; */
    GtkWidget   *box0;
    GtkWidget   *box1;
    GtkWidget   *box2;
    GtkWidget   *entry;
    GtkWidget   *label;
    GtkTooltips *tooltips;

    tooltips = gtk_tooltips_new();

    box0 = gtk_hbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(window, box0);
    gtk_widget_show(box0);

    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(box0), box1);
    gtk_widget_show(box1);

    box2 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(box0), box2);
    gtk_widget_show(box2);

    for (i = 0; i < nb_genOpt; i++) {
        label = gtk_label_new(generalOpt[i].label);
        gtk_widget_set_usize(label, 60, 21); 
        gtk_box_pack_start(GTK_BOX(box1), label, FALSE, TRUE, 0);
        gtk_widget_show(label);
    }
    for (i = 0; i < nb_genOpt; i++) {
        entry = gtk_entry_new();
        gtk_widget_set_usize(entry, 350, 21); 
        gtk_tooltips_set_tip(tooltips, entry, generalOpt[i].desc,(gchar *)NULL);
        gtk_box_pack_start(GTK_BOX(box2), entry, FALSE, TRUE, 0);
        gtk_widget_show(entry);
        generalOpt[i].text_widget = entry; 
    }
}

/* ================================================================= */
/*                                                                   */
/*             Create boolean options check buttons                  */
/*                                                                   */
/* ================================================================= */

/*
 * create_boolOptions_buttons
 * --------------------------
 * Creates all toggles buttons for boolean options
 */
static void
create_boolOptions_buttons(GtkScrolledWindow *parent)
{
    gint         i;
    GtkWidget   *box0;
    GtkWidget   *box1;
    GtkWidget   *box2;
    GtkWidget   *button;
    GtkTooltips *tooltips;

    tooltips = gtk_tooltips_new();

    box0 = gtk_hbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(parent, box0);
    gtk_widget_show(box0);

    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(box0), box1);
    gtk_widget_show(box1);

    box2 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(box0), box2);
    gtk_widget_show(box2);

    for (i = 0; i < nb_boolOpt; i++) {
        button = gtk_check_button_new_with_label(BoolOpt[i].label);
        gtk_tooltips_set_tip(tooltips, button, BoolOpt[i].desc,(gchar *)NULL);
        gtk_box_pack_start(GTK_BOX(i%2 ? box1 : box2), button, TRUE, TRUE, 0);
        gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           (GtkSignalFunc) bool_option_clicked_cb, 
                           (gpointer)i);
        gtk_widget_show(button);
    }
}

/* ================================================================= */
/*                                                                   */
/*                            Main function                          */
/*                                                                   */
/* ================================================================= */

gint
main(gint argc, gchar *argv[])
{
    GtkWidget  *xglockroot;
    GdkWindow  *gdkwin;
    Window     *xwin; 

    GtkWidget  *box0;
    GtkWidget  *box1;
    GtkWidget  *box2;
    GtkWidget  *box3;
    GtkWidget  *scrolled_win;
    GtkWidget  *list;
    GtkWidget  *list_item;
    GtkWidget  *button;
    GtkWidget  *separator;
    GtkWidget  *menubar;
    GtkWidget  *menuitem;
    GtkWidget  *menu;
    GtkWidget  *previewButton;
    GtkWidget  *notebook;
    GtkWidget  *frame;
    GtkWidget  *label;
    GtkTooltips *tooltips;
    gint        i;
    gchar       temp[100];

    /* Init for some data */
    nb_mode = sizeof (LockProcs) / sizeof (LockProcs[0]);
    nb_boolOpt = sizeof (BoolOpt) / sizeof (BoolOpt[0]);
    nb_genOpt = sizeof (generalOpt) / sizeof (generalOpt[0]);
    nb_fntColorOpt = sizeof (fntcolorOpt) / sizeof (fntcolorOpt[0]);

    gtk_init(&argc, &argv);
    gtk_rc_parse("simplerc");

    gdkvisual = gdk_visual_get_best();
    gdkcolormap = gdk_colormap_new(gdkvisual, 0); 

    xglockroot = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(xglockroot, "");
    gtk_window_set_title(GTK_WINDOW(xglockroot), "GTK+ XLock tester");
    gtk_window_set_wmclass (GTK_WINDOW (xglockroot), "XgLock", "XLock");
    
    tooltips = gtk_tooltips_new();
    
    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(xglockroot), box1);
    gtk_widget_show(box1);
    
    box0 = gtk_vbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(box0), 10);
    gtk_box_pack_start(GTK_BOX(box1), box0, TRUE, TRUE, 0);
    gtk_widget_show(box0);
    
    box2 = gtk_hbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(box2), 10);
    gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
    gtk_widget_show(box2);
    
    /* menu bar */
    menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(box0), menubar, FALSE, TRUE, 0);
    gtk_widget_show(menubar);
    
    /* File menu and File menu bar item */
    menu = create_file_menu(xglockroot); 
    menuitem = gtk_menu_item_new_with_label("File"); 
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
    gtk_widget_show(menuitem);
    
    /* File menu and File menu bar item */
    menu = create_help_menu(xglockroot); 
    menuitem = gtk_menu_item_new_with_label("Help"); 
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
    gtk_widget_show(menuitem);
    
    /* Third box containing preview button */
    box3 = gtk_hbox_new(FALSE, 10);
    gtk_widget_set_usize(box3, WINDOW_WIDTH+20, WINDOW_HEIGHT );
    gtk_container_border_width(GTK_CONTAINER(box3), 1);
    gtk_box_pack_start(GTK_BOX(box0), box3, TRUE, TRUE, 0);
    gtk_widget_show(box3);
    
    /* Preview button (button because I need here an X window) */
    previewButton = gtk_button_new_with_label( "Preview window" ); 
    gtk_widget_set_usize(previewButton, WINDOW_WIDTH, WINDOW_HEIGHT );
    gtk_box_pack_start(GTK_BOX(box3), previewButton, TRUE, FALSE, 10);
    gtk_tooltips_set_tip(tooltips, previewButton, "This is the preview window",(gchar *)NULL);
    gtk_widget_realize(previewButton); 
    gtk_widget_show(previewButton); 
    
    /* Getting X window id through GDK */
    gdkwin = GTK_WIDGET(previewButton)->window; 
    xwin =(Window *)GDK_WINDOW_XWINDOW( gdkwin ); 
    (void) sprintf(numberwidget, "%ld",(unsigned long)xwin );
    
    /* notebook */
    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(box2), notebook, TRUE, TRUE, 0);
    gtk_widget_show(notebook);
    
    /* list of modes */
    frame = gtk_frame_new("Choice of the mode");
    gtk_container_border_width(GTK_CONTAINER(frame), 10);
    gtk_widget_set_usize(frame, FRAME_WIDTH, FRAME_HEIGHT);
    
    /* list */
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
    gtk_widget_show(scrolled_win);
    
    list = gtk_list_new();
    gtk_list_set_selection_mode(GTK_LIST(list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), list);
    gtk_container_set_focus_vadjustment (GTK_CONTAINER (list),
                                         gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_win)));
    GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (scrolled_win)->vscrollbar, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, list, "Select the mode you want to try",(gchar *)NULL);
    
    for (i = 0; i < nb_mode; i++) {
        sprintf(temp, "%s :%s", LockProcs[i].cmdline_arg, LockProcs[i].desc);
        list_item = gtk_list_item_new_with_label(temp);
        gtk_container_add(GTK_CONTAINER(list), list_item);
        gtk_signal_connect (GTK_OBJECT (list_item), "select",
                            (GtkSignalFunc) mode_list_item_select_cb, 
                            (gpointer) ((long) i));
        gtk_signal_connect (GTK_OBJECT (list_item), "deselect",
                            (GtkSignalFunc) mode_list_item_unselect_cb, 
                            (gpointer) NULL);
        gtk_widget_show(list_item);
    }
    gtk_widget_show( list ); 
    
    label = gtk_label_new("choice the mode");
    gtk_widget_show(frame);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
    
    /* Options */
    
    frame = gtk_frame_new("Options booleans");
    gtk_container_border_width(GTK_CONTAINER(frame), 10);
    gtk_widget_set_usize(frame, FRAME_WIDTH, FRAME_HEIGHT);
    gtk_widget_show(frame);
    
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
    gtk_widget_show(scrolled_win);
    
    create_boolOptions_buttons(GTK_SCROLLED_WINDOW(scrolled_win));
    label = gtk_label_new("Options");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
    /* General Options */
    
    frame = gtk_frame_new("General Options");
    gtk_container_border_width(GTK_CONTAINER(frame), 10);
    gtk_widget_set_usize(frame, FRAME_WIDTH, FRAME_HEIGHT);
    gtk_widget_show(frame);
    
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
    gtk_widget_show(scrolled_win);
    
    create_genOptions_entries(GTK_SCROLLED_WINDOW(scrolled_win));
    label = gtk_label_new("General Options");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
    /* Color Options */
    
    frame = gtk_frame_new("Color Options");
    gtk_container_border_width(GTK_CONTAINER(frame), 10);
    gtk_widget_set_usize(frame, FRAME_WIDTH, FRAME_HEIGHT );
    gtk_widget_show(frame);
    
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
    gtk_widget_show(scrolled_win);
    
    create_fntColorOptions_entries(GTK_SCROLLED_WINDOW(scrolled_win));
    label = gtk_label_new("Font & Color Options");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
    
    /* end of notebook */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, TRUE, 0);
    gtk_widget_show(separator);
    
    /* Last Hbox for buttons */
    box2 = gtk_hbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(box2), 10);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);
    
    /* buttons */
    button = gtk_button_new_with_label("launch");
    gtk_widget_set_usize(button, 60, 30); 
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, button, "Launch xlock in selected mode",(gchar *)NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) launch_xlock,
                       (gpointer) NULL);
    gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 4);
    gtk_widget_show(button);

    /* launch in root button */
    button = gtk_button_new_with_label("launch in root");
    gtk_widget_set_usize(button, 95, 30); 
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, button, "Launch xlock in root window",(gchar *)NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) launch_xlock,
                       (gpointer) "-inroot" );
    gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 4);
    gtk_widget_show(button);

    /* launch -- in window -- button */
    button = gtk_button_new_with_label("launch in window");
    gtk_widget_set_usize(button, 115, 30); 
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_tooltips_set_tip(tooltips, button, "Launch xlock in a window",(gchar *)NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) launch_xlock,
                       (gpointer) "-inwindow" );
    gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 4);
    gtk_widget_show(button);

    /* exit button */
    button = gtk_button_new_with_label("exit");
    gtk_tooltips_set_tip(tooltips, button, "Exit xglock",(gchar *)NULL);
    gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                              (GtkSignalFunc) exit_xglock,
                              (gpointer)xglockroot );
    gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 4);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);
    
    gtk_widget_show(xglockroot);

    gtk_main();

    return 0;
}
