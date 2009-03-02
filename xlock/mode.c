#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)mode.c	4.07 97/11/24 xlockmore";

#endif
/*-
 * mode.c - Modes for xlock.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 01-Apr-97: Fixed memory leak in XStringListToTextProperty.
 * 23-Feb-96: Extensive revision to implement new mode hooks and stuff
 *		Ron Hitchens <ron@idiom.com>
 * 04-Sep-95: Moved over from mode.h (previously resource.h) with new
 *            "&*_opts" by Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 *
 */

#include "xlock.h"


/* -------------------------------------------------------------------- */

/*-
 * Mode options: If count, cycles, or size options are set to 1 ...
 * they are probably not used by the mode.
 * To remove unwanted modes put a line of "#if 0" and "#endif"
 * around the mode and remove mode from the generated "modes/Makefile"
 * and/or "modes/glx/Makefile".  When adding new modes, keep them in
 * alphabetical order for -sequential to work properly.
 */

#ifndef USE_MODULES

LockStruct  LockProcs[] =
{
#ifdef MODE_ant
	{"ant", init_ant, draw_ant, release_ant,
	 refresh_ant, init_ant, NULL, &ant_opts,
	 1000, -3, 40000, -12, 64, 1.0, "",
	 "Shows Langton's and Turk's generalized ants", 0, NULL},
#endif
#ifdef MODE_atlantis
	{"atlantis", init_atlantis, draw_atlantis, release_atlantis,
	 refresh_atlantis, change_atlantis, NULL, &atlantis_opts,
	 1000, 4, 100, 6000, 64, 1.0, "",
	 "Shows moving sharks/whales/dolphin", 0, NULL},
#endif
#ifdef MODE_ball
	{"ball", init_ball, draw_ball, release_ball,
	 refresh_ball, init_ball, NULL, &ball_opts,
	 10000, 10, 20, -100, 64, 1.0, "",
	 "Shows bouncing balls", 0, NULL},
#endif
#ifdef MODE_bat
	{"bat", init_bat, draw_bat, release_bat,
	 refresh_bat, init_bat, NULL, &bat_opts,
	 100000, -8, 1, 0, 64, 1.0, "",
	 "Shows bouncing flying bats", 0, NULL},
#endif
#ifdef MODE_billiards
	{"billiards", init_billiards, draw_billiards, release_billiards,
	 refresh_billiards, init_billiards, NULL, &billiards_opts,
	 200000, 6, 30, 1, 64, 0.3, "",
	 "Shows billiards", 0, NULL},
#endif
#ifdef MODE_blot
	{"blot", init_blot, draw_blot, release_blot,
	 refresh_blot, init_blot, NULL, &blot_opts,
	 200000, 6, 30, 1, 64, 0.3, "",
	 "Shows Rorschach's ink blot test", 0, NULL},
#endif
#ifdef MODE_bouboule
	{"bouboule", init_bouboule, draw_bouboule, release_bouboule,
	 refresh_bouboule, init_bouboule, NULL, &bouboule_opts,
	 10000, 100, 1, 15, 64, 1.0, "",
	 "Shows Mimi's bouboule of moving stars", 0, NULL},
#endif
#ifdef MODE_bounce
	{"bounce", init_bounce, draw_bounce, release_bounce,
	 refresh_bounce, init_bounce, NULL, &bounce_opts,
	 5000, -10, 1, 0, 64, 1.0, "",
	 "Shows bouncing footballs", 0, NULL},
#endif
#ifdef MODE_braid
	{"braid", init_braid, draw_braid, release_braid,
	 refresh_braid, init_braid, NULL, &braid_opts,
	 1000, 15, 100, -7, 64, 1.0, "",
	 "Shows random braids and knots", 0, NULL},
#endif
#ifdef MODE_bubble
	{"bubble", init_bubble, draw_bubble, release_bubble,
	 refresh_bubble, init_bubble, NULL, &bubble_opts,
	 100000, 25, 1, 100, 64, 0.6, "",
	 "Shows popping bubbles", 0, NULL},
#endif
#ifdef MODE_bubble3d
	{"bubble3d", init_bubble3d, draw_bubble3d, release_bubble3d,
	 draw_bubble3d, change_bubble3d, NULL, &bubble3d_opts,
	 1000, 1, 2, 1, 64, 1.0, "",
	 "Richard Jones's GL bubbles", 0, NULL},
#endif
#ifdef MODE_bug
	{"bug", init_bug, draw_bug, release_bug,
	 refresh_bug, init_bug, NULL, &bug_opts,
	 75000, 10, 32767, -4, 64, 1.0, "",
	 "Shows Palmiter's bug evolution and garden of Eden", 0, NULL},
#endif
#ifdef MODE_cage
	{"cage", init_cage, draw_cage, release_cage,
	 draw_cage, change_cage, NULL, &cage_opts,
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL},
#endif
#ifdef MODE_cartoon
	{"cartoon", init_cartoon, draw_cartoon, release_cartoon,
	 NULL, init_cartoon, NULL, &cartoon_opts,
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows bouncing cartoons", 0, NULL},
#endif
#ifdef MODE_clock
	{"clock", init_clock, draw_clock, release_clock,
	 refresh_clock, init_clock, NULL, &clock_opts,
	 100000, -16, 200, -200, 64, 1.0, "",
	 "Shows Packard's clock", 0, NULL},
#endif
#ifdef MODE_coral
	{"coral", init_coral, draw_coral, release_coral,
	 init_coral, init_coral, NULL, &coral_opts,
	 60000, -3, 1, 35, 64, 0.6, "",
	 "Shows a coral reef", 0, NULL},
#endif
#ifdef MODE_crystal
	{"crystal", init_crystal, draw_crystal, release_crystal,
	 refresh_crystal, init_crystal, NULL, &crystal_opts,
	 60000, -500, 200, -15, 64, 1.0, "",
	 "Shows polygons in 2D plane groups", 0, NULL},
#endif
#ifdef MODE_daisy
	{"daisy", init_daisy, draw_daisy, release_daisy,
	 refresh_daisy, init_daisy, NULL, &daisy_opts,
	 100000, 300, 350, 1, 64, 1.0, "",
	 "Shows a meadow of daisies", 0, NULL},
#endif
#ifdef MODE_dclock
	{"dclock", init_dclock, draw_dclock, release_dclock,
	 refresh_dclock, init_dclock, NULL, &dclock_opts,
	 10000, 1, 10000, 1, 64, 0.3, "",
	 "Shows a floating digital clock or message", 0, NULL},
#endif
#ifdef MODE_decay
	{"decay", init_decay, draw_decay, release_decay,
	 refresh_decay, init_decay, NULL, &decay_opts,
	 200000, 6, 30, 1, 64, 0.3, "",
	 "Shows a decaying screen", 0, NULL},
#endif
#ifdef MODE_deco
	{"deco", init_deco, draw_deco, release_deco,
	 init_deco, init_deco, NULL, &deco_opts,
	 1000000, -30, 2, -10, 64, 0.6, "",
	 "Shows art as ugly as sin", 0, NULL},
#endif
#ifdef MODE_demon
	{"demon", init_demon, draw_demon, release_demon,
	 refresh_demon, init_demon, NULL, &demon_opts,
	 50000, 0, 1000, -7, 64, 1.0, "",
	 "Shows Griffeath's cellular automata", 0, NULL},
#endif
#ifdef MODE_dilemma
	{"dilemma", init_dilemma, draw_dilemma, release_dilemma,
	 refresh_dilemma, init_dilemma, NULL, &dilemma_opts,
	 200000, -2, 1000, 0, 64, 1.0, "",
	 "Shows Lloyd's Prisoner's Dilemma simulation", 0, NULL},
#endif
#ifdef MODE_discrete
	{"discrete", init_discrete, draw_discrete, release_discrete,
	 refresh_discrete, init_discrete, NULL, &discrete_opts,
	 1000, 4096, 2500, 1, 64, 1.0, "",
	 "Shows various discrete maps", 0, NULL},
#endif
#ifdef MODE_drift
	{"drift", init_drift, draw_drift, release_drift,
	 refresh_drift, init_drift, NULL, &drift_opts,
	 10000, 30, 1, 1, 64, 1.0, "",
	 "Shows cosmic drifting flame fractals", 0, NULL},
#endif
#ifdef MODE_eyes
	{"eyes", init_eyes, draw_eyes, release_eyes,
	 refresh_eyes, init_eyes, NULL, &eyes_opts,
	 20000, -8, 5, 1, 64, 1.0, "",
	 "Shows eyes following a bouncing grelb", 0, NULL},
#endif
#ifdef MODE_fadeplot
	{"fadeplot", init_fadeplot, draw_fadeplot, release_fadeplot,
	 refresh_fadeplot, init_fadeplot, NULL, &fadeplot_opts,
	 30000, 10, 1500, 1, 64, 0.6, "",
	 "Shows a fading plot of sine squared", 0, NULL},
#endif
#ifdef MODE_flag
	{"flag", init_flag, draw_flag, release_flag,
	 refresh_flag, init_flag, NULL, &flag_opts,
	 50000, 1, 1000, -7, 64, 1.0, "",
	 "Shows a waving flag image", 0, NULL},
#endif
#ifdef MODE_flame
	{"flame", init_flame, draw_flame, release_flame,
	 refresh_flame, init_flame, NULL, &flame_opts,
	 750000, 20, 10000, 1, 64, 1.0, "",
	 "Shows cosmic flame fractals", 0, NULL},
#endif
#ifdef MODE_flow
	{"flow", init_flow, draw_flow, release_flow,
	 refresh_flow, init_flow, NULL, &flow_opts,
	 1000, 1024, 3000, 1, 64, 1.0, "",
	 "Shows dynamic strange attractors", 0, NULL},
#endif
#ifdef MODE_forest
	{"forest", init_forest, draw_forest, release_forest,
	 refresh_forest, init_forest, NULL, &forest_opts,
	 400000, 100, 200, 1, 64, 1.0, "",
	 "Shows binary trees of a fractal forest", 0, NULL},
#endif
#ifdef MODE_galaxy
	{"galaxy", init_galaxy, draw_galaxy, release_galaxy,
	 refresh_galaxy, init_galaxy, NULL, &galaxy_opts,
	 100, -5, 250, -3, 64, 1.0, "",
	 "Shows crashing spiral galaxies", 0, NULL},
#endif
#ifdef MODE_gears
	{"gears", init_gears, draw_gears, release_gears,
	 draw_gears, init_gears, NULL, &gears_opts,
	 1000, 1, 2, 1, 64, 1.0, "",
	 "Shows GL's gears", 0, NULL},
#endif
#ifdef MODE_goop
	{"goop", init_goop, draw_goop, release_goop,
	 init_goop, init_goop, NULL, &goop_opts,
	 10000, -12, 1, 1, 64, 1.0, "",
	 "Shows goop from a lava lamp", 0, NULL},
#endif
#ifdef MODE_grav
	{"grav", init_grav, draw_grav, release_grav,
	 refresh_grav, init_grav, NULL, &grav_opts,
	 10000, -12, 1, 1, 64, 1.0, "",
	 "Shows orbiting planets", 0, NULL},
#endif
#ifdef MODE_helix
	{"helix", init_helix, draw_helix, release_helix,
	 refresh_helix, init_helix, NULL, &helix_opts,
	 25000, 1, 100, 1, 64, 1.0, "",
	 "Shows string art", 0, NULL},
#endif
#ifdef MODE_hop
	{"hop", init_hop, draw_hop, release_hop,
	 refresh_hop, init_hop, NULL, &hop_opts,
	 10000, 1000, 2500, 1, 64, 1.0, "",
	 "Shows real plane iterated fractals", 0, NULL},
#endif
#ifdef MODE_hyper
	{"hyper", init_hyper, draw_hyper, release_hyper,
	 refresh_hyper, change_hyper, NULL, &hyper_opts,
	 100000, -6, 300, 1, 64, 1.0, "",
	 "Shows spinning n-dimensional hypercubes", 0, NULL},
#endif
#ifdef MODE_ico
	{"ico", init_ico, draw_ico, release_ico,
	 refresh_ico, change_ico, NULL, &ico_opts,
	 100000, 0, 400, 0, 64, 1.0, "",
	 "Shows a bouncing polyhedron", 0, NULL},
#endif
#ifdef MODE_ifs
	{"ifs", init_ifs, draw_ifs, release_ifs,
	 init_ifs, init_ifs, NULL, &ifs_opts,
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows a modified iterated function system", 0, NULL},
#endif
#ifdef MODE_image
	{"image", init_image, draw_image, release_image,
	 refresh_image, init_image, NULL, &image_opts,
	 3000000, -20, 1, 1, 64, 1.0, "",
	 "Shows randomly appearing logos", 0, NULL},
#endif
#ifdef MODE_invert
	{"invert", init_invert, draw_invert, release_invert,
	 draw_invert, init_invert, NULL, &invert_opts,
	 100, 1, 1, 1, 64, 1.0, "",
	 "Shows a sphere inverted without wrinkles", 0, NULL},
#endif
#ifdef MODE_juggle
	{"juggle", init_juggle, draw_juggle, release_juggle,
	 draw_juggle, init_juggle, NULL, &juggle_opts,
	 2000, 30, 30, 1, 64, 1.0, "",
	 "Shows a Juggler, juggling", 0, NULL},
#endif
#ifdef MODE_julia
	{"julia", init_julia, draw_julia, release_julia,
	 refresh_julia, init_julia, NULL, &julia_opts,
	 10000, 1000, 20, 1, 64, 1.0, "",
	 "Shows the Julia set", 0, NULL},
#endif
#ifdef MODE_kaleid
	{"kaleid", init_kaleid, draw_kaleid, release_kaleid,
	 refresh_kaleid, init_kaleid, NULL, &kaleid_opts,
	 80000, 4, 40, -9, 64, 0.6, "",
	 "Shows a kaleidoscope", 0, NULL},
#endif
#ifdef MODE_kumppa
	{"kumppa", init_kumppa, draw_kumppa, release_kumppa,
	 refresh_kumppa, init_kumppa, NULL, &kumppa_opts,
	 10000, 1, 1000, 1, 64, 1.0, "",
	 "Shows kumppa", 0, NULL},
#endif
#ifdef MODE_lament
	{"lament", init_lament, draw_lament, release_lament,
	 draw_lament, change_lament, NULL, &lament_opts,
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows Lemarchand's Box", 0, NULL},
#endif
#ifdef MODE_laser
	{"laser", init_laser, draw_laser, release_laser,
	 refresh_laser, init_laser, NULL, &laser_opts,
	 20000, -10, 200, 1, 64, 1.0, "",
	 "Shows spinning lasers", 0, NULL},
#endif
#ifdef MODE_life
	{"life", init_life, draw_life, release_life,
	 refresh_life, change_life, NULL, &life_opts,
	 750000, 40, 140, 0, 64, 1.0, "",
	 "Shows Conway's game of Life", 0, NULL},
#endif
#ifdef MODE_life1d
	{"life1d", init_life1d, draw_life1d, release_life1d,
	 refresh_life1d, init_life1d, NULL, &life1d_opts,
	 10000, 1, 10, 0, 64, 1.0, "",
	 "Shows Wolfram's game of 1D Life", 0, NULL},
#endif
#ifdef MODE_life3d
	{"life3d", init_life3d, draw_life3d, release_life3d,
	 refresh_life3d, change_life3d, NULL, &life3d_opts,
	 1000000, 35, 85, 1, 64, 1.0, "",
	 "Shows Bays' game of 3D Life", 0, NULL},
#endif
#ifdef MODE_lightning
	{"lightning", init_lightning, draw_lightning, release_lightning,
	 refresh_lightning, init_lightning, NULL, &lightning_opts,
	 10000, 1, 1, 1, 64, 0.6, "",
	 "Shows Keith's fractal lightning bolts", 0, NULL},
#endif
#ifdef MODE_lisa
	{"lisa", init_lisa, draw_lisa, release_lisa,
	 refresh_lisa, change_lisa, NULL, &lisa_opts,
	 25000, 1, 256, -1, 64, 1.0, "",
	 "Shows animated lisajous loops", 0, NULL},
#endif
#ifdef MODE_lissie
	{"lissie", init_lissie, draw_lissie, release_lissie,
	 refresh_lissie, init_lissie, NULL, &lissie_opts,
	 10000, 1, 2000, -200, 64, 0.6, "",
	 "Shows lissajous worms", 0, NULL},
#endif
#ifdef MODE_loop
	{"loop", init_loop, draw_loop, release_loop,
	 refresh_loop, init_loop, NULL, &loop_opts,
	 100000, 1, 1600, -12, 64, 1.0, "",
	 "Shows Langton's self-producing loops", 0, NULL},
#endif
#ifdef MODE_lyapunov
	{"lyapunov", init_lyapunov, draw_lyapunov, release_lyapunov,
	 NULL, init_lyapunov, NULL, &lyapunov_opts,
	 25000, 600, 1, 1, 64, 1.0, "",
	 "Shows lyapunov space", 0, NULL},
#endif
#ifdef MODE_mandelbrot
	{"mandelbrot", init_mandelbrot, draw_mandelbrot, release_mandelbrot,
	 NULL, init_mandelbrot, NULL, &mandelbrot_opts,
	 25000, -8, 20000, 1, 64, 1.0, "",
	 "Shows mandelbrot sets", 0, NULL},
#endif
#ifdef MODE_marquee
	{"marquee", init_marquee, draw_marquee, release_marquee,
	 init_marquee, init_marquee, NULL, &marquee_opts,
	 100000, 1, 1, 1, 64, 1.0, "",
	 "Shows messages", 0, NULL},
#endif
#ifdef MODE_matrix
	{"matrix", init_matrix, draw_matrix, release_matrix,
	 refresh_matrix, change_matrix, NULL, &matrix_opts,
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows the Matrix", 0, NULL},
#endif
#ifdef MODE_maze
	{"maze", init_maze, draw_maze, release_maze,
	 refresh_maze, init_maze, NULL, &maze_opts,
	 1000, 1, 3000, -40, 64, 1.0, "",
	 "Shows a random maze and a depth first search solution", 0, NULL},
#endif
#ifdef MODE_moebius
	{"moebius", init_moebius, draw_moebius, release_moebius,
	 draw_moebius, change_moebius, NULL, &moebius_opts,
	 1000, 1, 1, 1, 64, 1.0, "",
       "Shows Moebius Strip II, an Escher-like GL scene with ants", 0, NULL},
#endif
#ifdef MODE_morph3d
	{"morph3d", init_morph3d, draw_morph3d, release_morph3d,
	 draw_morph3d, change_morph3d, NULL, &morph3d_opts,
	 1000, 0, 1, 1, 64, 1.0, "",
	 "Shows GL morphing polyhedra", 0, NULL},
#endif
#ifdef MODE_mountain
	{"mountain", init_mountain, draw_mountain, release_mountain,
	 refresh_mountain, init_mountain, NULL, &mountain_opts,
	 1000, 30, 4000, 1, 64, 1.0, "",
	 "Shows Papo's mountain range", 0, NULL},
#endif
#ifdef MODE_munch
	{"munch", init_munch, draw_munch, release_munch,
	 init_munch, init_munch, NULL, &munch_opts,
	 5000, 1, 7, 1, 64, 1.0, "",
	 "Shows munching squares", 0, NULL},
#endif
#ifdef MODE_nose
	{"nose", init_nose, draw_nose, release_nose,
	 refresh_nose, init_nose, NULL, &nose_opts,
	 100000, 1, 1, 1, 64, 1.0, "",
    "Shows a man with a big nose runs around spewing out messages", 0, NULL},
#endif
#ifdef MODE_pacman
	{"pacman", init_pacman, draw_pacman, release_pacman,
	 refresh_pacman, init_pacman, NULL, &pacman_opts,
	 100000, 10, 1, 0, 64, 1.0, "",
	 "Shows Pacman(tm)", 0, NULL},
#endif
#ifdef MODE_penrose
	{"penrose", init_penrose, draw_penrose, release_penrose,
	 init_penrose, init_penrose, NULL, &penrose_opts,
	 10000, 1, 1, -40, 64, 1.0, "",
	 "Shows Penrose's quasiperiodic tilings", 0, NULL},
#endif
#ifdef MODE_petal
	{"petal", init_petal, draw_petal, release_petal,
	 refresh_petal, init_petal, NULL, &petal_opts,
	 10000, -500, 400, 1, 64, 1.0, "",
	 "Shows various GCD Flowers", 0, NULL},
#endif
#ifdef MODE_pipes
	{"pipes", init_pipes, draw_pipes, release_pipes,
#if defined( MESA ) && defined( SLOW )
	 draw_pipes,
#else
	 change_pipes,
#endif
	 change_pipes, NULL, &pipes_opts,
	 1000, 2, 5, 500, 64, 1.0, "",
	 "Shows a selfbuilding pipe system", 0, NULL},
#endif
#ifdef MODE_puzzle
	{"puzzle", init_puzzle, draw_puzzle, release_puzzle,
	 init_puzzle, init_puzzle, NULL, &puzzle_opts,
	 10000, 250, 1, 1, 64, 1.0, "",
	 "Shows a puzzle being scrambled and then solved", 0, NULL},
#endif
#ifdef MODE_pyro
	{"pyro", init_pyro, draw_pyro, release_pyro,
	 refresh_pyro, init_pyro, NULL, &pyro_opts,
	 15000, 100, 1, -3, 64, 1.0, "",
	 "Shows fireworks", 0, NULL},
#endif
#ifdef MODE_qix
	{"qix", init_qix, draw_qix, release_qix,
	 refresh_qix, init_qix, NULL, &qix_opts,
	 30000, -5, 32, 1, 64, 1.0, "",
	 "Shows spinning lines a la Qix(tm)", 0, NULL},
#endif
#ifdef MODE_roll
	{"roll", init_roll, draw_roll, release_roll,
	 refresh_roll, init_roll, NULL, &roll_opts,
	 100000, 25, 1, -64, 64, 0.6, "",
	 "Shows a rolling ball", 0, NULL},
#endif
#ifdef MODE_rotor
	{"rotor", init_rotor, draw_rotor, release_rotor,
	 refresh_rotor, init_rotor, NULL, &rotor_opts,
	 500, 4, 100, -6, 64, 0.3, "",
	 "Shows Tom's Roto-Rooter", 0, NULL},
#endif
#ifdef MODE_rubik
	{"rubik", init_rubik, draw_rubik, release_rubik,
	 draw_rubik, change_rubik, NULL, &rubik_opts,
	 10000, -30, 5, -6, 64, 1.0, "",
	 "Shows an auto-solving Rubik's Cube", 0, NULL},
#endif
#ifdef MODE_shape
	{"shape", init_shape, draw_shape, release_shape,
	 refresh_shape, init_shape, NULL, &shape_opts,
	 10000, 100, 256, 1, 64, 1.0, "",
	 "Shows stippled rectangles, ellipses, and triangles", 0, NULL},
#endif
#ifdef MODE_sierpinski
	{"sierpinski", init_sierpinski, draw_sierpinski, release_sierpinski,
	 refresh_sierpinski, init_sierpinski, NULL, &sierpinski_opts,
	 400000, 2000, 100, 1, 64, 1.0, "",
	 "Shows Sierpinski's triangle", 0, NULL},
#endif
#ifdef MODE_slip
	{"slip", init_slip, draw_slip, release_slip,
	 init_slip, init_slip, NULL, &slip_opts,
	 50000, 35, 50, 1, 64, 1.0, "",
	 "Shows slipping blits", 0, NULL},
#endif
#ifdef MODE_solitare
	{"solitare", init_solitare, draw_solitare, release_solitare,
	 refresh_solitare, init_solitare, NULL, &solitare_opts,
	 2000000, 1, 1, 1, 64, 1.0, "",
	 "Shows Klondike's game of solitare", 0, NULL},
#endif
#ifdef MODE_space
	{"space", init_space, draw_space, release_space,
	 refresh_space, init_space, NULL, &space_opts,
	 10000, 100, 1, 15, 64, 1.0, "",
	 "Shows a journey into deep space", 0, NULL},
#endif
#ifdef MODE_sphere
	{"sphere", init_sphere, draw_sphere, release_sphere,
	 refresh_sphere, init_sphere, NULL, &sphere_opts,
	 5000, 1, 20, 0, 64, 1.0, "",
	 "Shows a bunch of shaded spheres", 0, NULL},
#endif
#ifdef MODE_spiral
	{"spiral", init_spiral, draw_spiral, release_spiral,
	 refresh_spiral, init_spiral, NULL, &spiral_opts,
	 5000, -40, 350, 1, 64, 1.0, "",
	 "Shows a helical locus of points", 0, NULL},
#endif
#ifdef MODE_spline
	{"spline", init_spline, draw_spline, release_spline,
	 refresh_spline, init_spline, NULL, &spline_opts,
	 30000, -6, 2048, 1, 64, 0.3, "",
	 "Shows colorful moving splines", 0, NULL},
#endif
#ifdef MODE_sproingies
	{"sproingies", init_sproingies, draw_sproingies, release_sproingies,
	 refresh_sproingies, init_sproingies, NULL, &sproingies_opts,
	 1000, 5, 0, 400, 64, 1.0, "",
  "Shows Sproingies!  Nontoxic.  Safe for pets and small children", 0, NULL},
#endif
#ifdef MODE_stairs
	{"stairs", init_stairs, draw_stairs, release_stairs,
	 draw_stairs, change_stairs, NULL, &stairs_opts,
	 200000, 0, 1, 1, 64, 1.0, "",
	 "Shows some Infinite Stairs, an Escher-like scene", 0, NULL},
#endif
#ifdef MODE_star
	{"star", init_star, draw_star, release_star,
	 refresh_star, init_star, NULL, &star_opts,
	 75000, 100, 1, 100, 64, 0.3, "",
	 "Shows a star field with a twist", 0, NULL},
#endif
#ifdef MODE_starfish
	{"starfish", init_starfish, draw_starfish, release_starfish,
	 refresh_starfish, init_starfish, NULL, &starfish_opts,
	 2000, 1, 1000, 1, 64, 1.0, "",
	 "Shows starfish", 0, NULL},
#endif
#ifdef MODE_strange
	{"strange", init_strange, draw_strange, release_strange,
	 init_strange, init_strange, NULL, &strange_opts,
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows strange attractors", 0, NULL},
#endif
#ifdef MODE_superquadrics
	{"superquadrics", init_superquadrics, draw_superquadrics, release_superquadrics,
	 refresh_superquadrics, init_superquadrics, NULL, &superquadrics_opts,
	 1000, 25, 40, 1, 64, 1.0, "",
	 "Shows 3D mathematical shapes", 0, NULL},
#endif
#ifdef MODE_swarm
	{"swarm", init_swarm, draw_swarm, release_swarm,
	 refresh_swarm, init_swarm, NULL, &swarm_opts,
	 15000, 100, 1, 1, 64, 1.0, "",
	 "Shows a swarm of bees following a wasp", 0, NULL},
#endif
#ifdef MODE_swirl
	{"swirl", init_swirl, draw_swirl, release_swirl,
	 refresh_swirl, init_swirl, NULL, &swirl_opts,
	 5000, 5, 1, 1, 64, 1.0, "",
	 "Shows animated swirling patterns", 0, NULL},
#endif
#ifdef MODE_t3d
	{"t3d", init_t3d, draw_t3d, release_t3d,
	 refresh_t3d, init_t3d, NULL, &t3d_opts,
	 250000, 1, 60000, 1, 64, 1.0, "",
	 "Shows a Flying Balls Clock Demo", 0, NULL},
#endif
#ifdef MODE_tetris
	{"tetris", init_tetris, draw_tetris, release_tetris,
	 refresh_tetris, init_tetris, NULL, &tetris_opts,
	 50000, 1, 1, 0, 64, 1.0, "",
	 "Shows an autoplaying tetris game", 0, NULL},
#endif
#ifdef MODE_text3d
	{"text3d", init_text3d, draw_text3d, release_text3d,
	 refresh_text3d, change_text3d, NULL, &text3d_opts,
	 100000, 1, 1, 1, 64, 1.0, "",
	 "Shows 3D text", 0, NULL},
#endif
#ifdef MODE_thornbird
	{"thornbird", init_thornbird, draw_thornbird, release_thornbird,
	 refresh_thornbird, init_thornbird, NULL, &thornbird_opts,
	 1000, 800, 16, 1, 64, 1.0, "",
	 "Shows an animated bird in a thorn bush fractal map", 0, NULL},
#endif
#ifdef MODE_tik_tak
  {"tik_tak", init_tik_tak, draw_tik_tak, release_tik_tak,
   refresh_tik_tak, init_tik_tak, NULL, &tik_tak_opts,
   60000, -20, 200, -1000, 64, 1.0, "",
   "Shows rotating polygons", 0, NULL},
#endif
#ifdef MODE_triangle
	{"triangle", init_triangle, draw_triangle, release_triangle,
	 refresh_triangle, init_triangle, NULL, &triangle_opts,
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows a triangle mountain range", 0, NULL},
#endif
#ifdef MODE_tube
	{"tube", init_tube, draw_tube, release_tube,
	 NULL, init_tube, NULL, &tube_opts,
	 25000, -9, 20000, -200, 64, 1.0, "",
	 "Shows an animated tube", 0, NULL},
#endif
#ifdef MODE_turtle
	{"turtle", init_turtle, draw_turtle, release_turtle,
	 init_turtle, init_turtle, NULL, &turtle_opts,
	 1000000, 1, 20, 1, 64, 1.0, "",
	 "Shows turtle fractals", 0, NULL},
#endif
#ifdef MODE_vines
	{"vines", init_vines, draw_vines, release_vines,
	 refresh_vines, init_vines, NULL, &vines_opts,
	 200000, 0, 1, 1, 64, 1.0, "",
	 "Shows fractals", 0, NULL},
#endif
#ifdef MODE_voters
	{"voters", init_voters, draw_voters, release_voters,
	 refresh_voters, init_voters, NULL, &voters_opts,
	 1000, 0, 327670, 0, 64, 1.0, "",
	 "Shows Dewdney's Voters", 0, NULL},
#endif
#ifdef MODE_wator
	{"wator", init_wator, draw_wator, release_wator,
	 refresh_wator, init_wator, NULL, &wator_opts,
	 750000, 1, 32767, 0, 64, 1.0, "",
	 "Shows Dewdney's Water-Torus planet of fish and sharks", 0, NULL},
#endif
#ifdef MODE_wire
	{"wire", init_wire, draw_wire, release_wire,
	 refresh_wire, init_wire, NULL, &wire_opts,
	 500000, 1000, 150, -8, 64, 1.0, "",
	 "Shows a random circuit with 2 electrons", 0, NULL},
#endif
#ifdef MODE_world
	{"world", init_world, draw_world, release_world,
	 refresh_world, init_world, NULL, &world_opts,
	 100000, -16, 1, 1, 64, 0.3, "",
	 "Shows spinning Earths", 0, NULL},
#endif
#ifdef MODE_worm
	{"worm", init_worm, draw_worm, release_worm,
	 refresh_worm, init_worm, NULL, &worm_opts,
	 17000, -20, 10, -3, 64, 1.0, "",
	 "Shows wiggly worms", 0, NULL},
#endif
#ifdef MODE_xjack
	{"xjack", init_xjack, draw_xjack, release_xjack,
	 init_xjack, init_xjack, NULL, &xjack_opts,
	 50000, 1, 1, 1, 64, 1.0, "",
	 "Shows Jack having one of those days", 0, NULL},
#endif

/* SPECIAL MODES */
	{"blank", init_blank, draw_blank, release_blank,
	 refresh_blank, init_blank, NULL, &blank_opts,
	 3000000, 1, 1, 1, 64, 1.0, "",
	 "Shows nothing but a black screen", 0, NULL},
#ifdef MODE_bomb
	{"bomb", init_bomb, draw_bomb, release_bomb,
	 refresh_bomb, change_bomb, NULL, &bomb_opts,
	 100000, 10, 20, 1, 64, 1.0, "",
	 "Shows a bomb and will autologout after a time", 0, NULL},
	{"random", init_random, draw_random, release_random,
	 refresh_random, change_random, NULL, &random_opts,
	 1, 1, 1, 1, 64, 1.0, "",
	 "Shows a random mode (except blank and bomb)", 0, NULL},
#else
	{"random", init_random, draw_random, release_random,
	 refresh_random, change_random, NULL, &random_opts,
	 1, 1, 1, 1, 64, 1.0, "",
	 "Shows a random mode (except blank)", 0, NULL},
#endif
};

int         numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

#else /* #ifndef USE_MODULES */

#include <sys/stat.h>
#include <pwd.h>
#include <dlfcn.h>
#include <errno.h>

LockStruct *LockProcs = NULL;
void      **LoadedModules = NULL;	/* save module handles to unload them later */
int         numprocs = 0;

/*-
 * I use a stack to save information about each module until they are
 * all loaded because I can not allocate LockProcs until I know how many
 * modules there are.
 */
typedef struct stack {
	struct stack *next;
	LockStruct *lock;
	void       *mod;	/* pointer to module */
} stack;

/*-
 * compare function to use with qsort, to sort the list of modules
 * after loading them.
 */
static int
lock_compare(const void *a, const void *b)
{
	const LockStruct *A = *((LockStruct **) a), *B = *((LockStruct **) b);

	return strcmp(A->cmdline_arg, B->cmdline_arg);
}

/*-
 * Does tilde expansion on path, and expands any occurrence of %S to
 * the default modulepath.  Returns a pointer to a static string
 * containing the expanded path.  This will be overwritten by
 * subsequent calls to ExpandPath.
 */
static char *
ExpandPath(char *path)
{
	static char expandedpath[1024];
	char       *ep = expandedpath;

	if (path[0] == '~') {	/* find homedir */
		int         i = 0;
		char        user[128], *homedir;
		struct passwd *pw = NULL;

		path++;
		while (*path && *path != '/')
			user[i++] = *path++;
		user[i] = '\0';

		if (user[0] == '\0') {	/* get this user's homedir */
			homedir = getenv("HOME");
			if (!homedir) {		/* try password entry */
				pw = getpwuid(getuid());
				if (!pw) {
					char       *login = getlogin();

					if (login)
						pw = getpwnam(login);
				}
				if (pw)
					homedir = pw->pw_dir;
				else
					return NULL;	/* can't expand name */
			}
		} else {	/* refers to another user's home dir */
			pw = getpwnam(user);
			if (pw)
				homedir = pw->pw_dir;
			else
				return NULL;
		}
		(void) strcpy(ep, homedir);
		ep += strlen(homedir);
	}			/* if (path[0] ..  */
	while (*path) {
		switch (*path) {
			case '%':
				path++;
				switch (*path) {
					case 'S':
						*ep = '\0';
						(void) strcat(ep, DEF_MODULEPATH);
						ep += strlen(DEF_MODULEPATH);
						break;
					case '%':
						*ep++ = *path++;
						break;
					default:
						path++;
						break;
				}

			default:
				*ep++ = *path++;
				break;
		}
	}

	*ep = '\0';
	return expandedpath;
}

/*-
 * Loads screensaver modules in the directories in path.  path is a
 * colon separated list of direcories to search in.  LoadModules
 * searches for files ending in .xlk to load as modules.
 */

#define nextone (void) fprintf(stderr, "LoadModule: %s: %s\n", cpath, dlerror()); \
(void) dlclose(mp); \
(void) free((void *) newstack->lock); \
(void) free((void *) newstack); \
continue

void
LoadModules(char *path)
{
	DIR        *dp;
	struct stat st;
	struct dirent *ent;
	char        cpath[128], *p, *thisp;
	stack      *newstack, *head = NULL;
	int         count = 0;

	if (path) {
		p = (char *) malloc(strlen(path) + 1);
		(void) strcpy(p, path);
	} else {
		p = (char *) malloc(1);
		p[0] = '\0';
		(void) fprintf(stderr, "%s: LoadModules: modulepath is null\n",
			       ProgramName);
	}
	for (thisp = strtok(p, ": \t"); thisp != NULL; thisp = strtok(NULL, ": \t")) {
		char       *ep;

		ep = ExpandPath(thisp);
		if (ep == NULL) {
			(void) fprintf(stderr, "%s: LoadModules: Can not expand path - '%s'\n",
				       ProgramName, thisp);
			continue;
		} else
			thisp = ep;

		dp = opendir(thisp);
		if (dp == NULL) {
			(void) fprintf(stderr, "%s: LoadModules: %s: %s\n", ProgramName,
				       thisp, strerror(errno));
			continue;
		}
		while ((ent = readdir(dp)) != NULL) {
			int         len;
			void       *mp;

			if ((len = strlen(ent->d_name)) > 4) {
				char       *suf = &ent->d_name[len - 4];	/* check suffix */

				if (strcmp(suf, ".xlk") != 0)
					continue;
			} else
				continue;

			(void) sprintf(cpath, "%s%s%s", thisp,
				(thisp[strlen(thisp) - 1] == '/') ? "" : "/",
				       ent->d_name);
			if (stat(cpath, &st) != 0) {
				(void) fprintf(stderr, "%s: LoadModules: %s: %s\n", ProgramName,
					       cpath, strerror(errno));
				continue;
			}
			if (!S_ISREG(st.st_mode))	/* make sure it's not a directory */
				continue;

			mp = dlopen(cpath, RTLD_NOW);	/* load module */
			if (mp == NULL) {
				(void) fprintf(stderr, "%s: LoadModule: %s: %s\n", ProgramName,
					       cpath, dlerror());
				continue;
			} else {
				char        descsym[32];
				ModStruct  *desc;

				/*
				 * The name of the description structure is formed by concatenating
				 * the name of the module minus the .xlk suffix, and the string
				 * "_description".
				 */
				(void) sprintf(descsym, "%.*s_description", len - 4, ent->d_name);
				desc = (ModStruct *) dlsym(mp, descsym);
				if (desc == NULL) {
					(void) fprintf(stderr, "LoadModule: %s: %s\n", descsym, dlerror());
					(void) dlclose(mp);
					continue;
				}
				/* save information about module on stack. */
				newstack = (stack *) malloc(sizeof (stack));
				newstack->mod = mp;
				newstack->lock = (LockStruct *) malloc(sizeof (LockStruct));
				newstack->lock->cmdline_arg = desc->cmdline_arg;

				if (desc->init_name != NULL) {
					newstack->lock->init_hook = (ModeHook *) dlsym(mp, (char *) desc->init_name);
					if (newstack->lock->init_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->init_hook = NULL;
				if (desc->callback_name != NULL) {
					newstack->lock->callback_hook = (ModeHook *) dlsym(mp, desc->callback_name);
					if (newstack->lock->callback_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->callback_hook = NULL;
				if (desc->release_name != NULL) {
					newstack->lock->release_hook = (ModeHook *) dlsym(mp, desc->release_name);
					if (newstack->lock->release_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->refresh_hook = NULL;
				if (desc->refresh_name != NULL) {
					newstack->lock->refresh_hook = (ModeHook *) dlsym(mp, desc->refresh_name);
					if (newstack->lock->refresh_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->refresh_hook = NULL;
				if (desc->change_name != NULL) {
					newstack->lock->change_hook = (ModeHook *) dlsym(mp, desc->change_name);
					if (newstack->lock->change_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->change_hook = NULL;
				if (desc->unused_name != NULL) {
					newstack->lock->unused_hook = (ModeHook *) dlsym(mp, desc->unused_name);
					if (newstack->lock->unused_hook == NULL) {
						nextone;
					}
				} else
					newstack->lock->unused_hook = NULL;
				newstack->lock->msopt = desc->msopt;
				newstack->lock->def_delay = desc->def_delay;
				newstack->lock->def_count = desc->def_count;
				newstack->lock->def_cycles = desc->def_cycles;
				newstack->lock->def_size = desc->def_size;
				newstack->lock->def_ncolors = desc->def_ncolors;
				newstack->lock->def_saturation = desc->def_saturation;
				newstack->lock->def_bitmap = desc->def_bitmap;
				newstack->lock->desc = desc->desc;
				newstack->lock->flags = desc->flags;
				newstack->lock->userdata = desc->userdata;

				newstack->next = head;
				head = newstack;
				count++;
			}	/* if (mp == NULL) .. else */
		}		/* while ((ent = ...) */

		(void) closedir(dp);
	}			/* for (thisp = ... */
	(void) free((void *) p);

	if (count > 0) {
		int         i;
		stack      *discard;
		LockStruct **locks;

		LoadedModules = (void **) malloc(count * sizeof (void *));

		LockProcs = (LockStruct *) malloc(count * sizeof (LockStruct));
		locks = (LockStruct **) malloc(count * sizeof (LockStruct *));

		/* Copy module info from stack to locks array temporarily to
		 * sort them. Then copy them to LockProcs.  Also save module
		 * handles in LoadedModules, to be used by UnloadModules()
		 * later.
		 */
		for (i = 0; i < count; i++) {
			LoadedModules[i] = head->mod;
			locks[i] = head->lock;
			discard = head;
			head = head->next;
			(void) free((void *) discard);
		}
		(void) qsort((void *) locks, count, sizeof (LockStruct *), lock_compare);
		for (i = 0; i < count; i++) {
			(void) memcpy(&LockProcs[i], locks[i], sizeof (LockStruct));
			(void) free((void *) locks[i]);
		}
		(void) free((void *) locks);
	}
	numprocs = count;
}

void
UnloadModules()
{
	int         i;

	if (LoadedModules != NULL) {
		for (i = 0; i < numprocs; i++)
			(void) dlclose(LoadedModules[i]);

		(void) free((void *) LoadedModules);
	}
	if (LockProcs != NULL)
		(void) free((void *) LockProcs);
}

#endif /* #ifndef USE_MODULES ... #else */

/* -------------------------------------------------------------------- */

static LockStruct *last_initted_mode = NULL;
static LockStruct *default_mode = NULL;

/* -------------------------------------------------------------------- */

void
set_default_mode(LockStruct * ls)
{
	default_mode = ls;
}

/* -------------------------------------------------------------------- */

static void
set_window_title(ModeInfo * mi)
{
	XTextProperty prop;
	char        buf[512];
	char       *ptr = buf;
	unsigned int status;
	extern Bool description;

	(void) sprintf(buf, "%s: %s", MI_NAME(mi), MI_DESC(mi));
	status = XStringListToTextProperty(&ptr, 1, &prop);
	if (status != 0) {
		XSetWMName(MI_DISPLAY(mi), MI_WINDOW(mi), &prop);
		XFree((caddr_t) prop.value);
	}
	if (MI_IS_ICONIC(mi) && description) {
		modeDescription(mi);
	}
}

/* -------------------------------------------------------------------- */

/*-
 *    This hook is called prior to calling the init hook of a
 *      different mode.  It is to inform the mode that it is losing
 *      control, and should therefore release any dynamically created
 *      resources.
 */

void
call_release_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		return;
	}
	MI_LOCKSTRUCT(mi) = ls;
	if (ls->release_hook != NULL) {
		ls->release_hook(mi);
	}
	ls->flags &= ~(LS_FLAG_INITED);

	last_initted_mode = NULL;
}

void
release_last_mode(ModeInfo * mi)
{
	if (last_initted_mode == NULL) {
		return;
	}
	call_release_hook(last_initted_mode, mi);

	last_initted_mode = NULL;
}

/* -------------------------------------------------------------------- */

/*-
 *    Call the init hook for a mode.  If this mode is not the same
 *      as the last one, call the release proc for the previous mode
 *      so that it will surrender its dynamic resources.
 *      A mode's init hook may be called multiple times, without
 *      intervening release calls.
 */

void
call_init_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	if (ls != last_initted_mode) {
		call_release_hook(last_initted_mode, mi);
	}
	MI_LOCKSTRUCT(mi) = ls;
	set_window_title(mi);

	ls->init_hook(mi);

	ls->flags |= LS_FLAG_INITED;
	MI_SET_FLAG_STATE(mi, WI_FLAG_JUST_INITTED, True);

	last_initted_mode = ls;
}

/* -------------------------------------------------------------------- */

/*-
 *    Call the callback hook for a mode.  This hook is called repeatedly,
 *      at (approximately) constant time intervals.  The time between calls
 *      is controlled by the -delay command line option, which is mapped
 *      to the variable named delay.
 */

void
call_callback_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	ls->callback_hook(mi);

	MI_SET_FLAG_STATE(mi, WI_FLAG_JUST_INITTED, False);
}

/* -------------------------------------------------------------------- */

/*-
 *    Window damage has occurred.  If the mode has been initted and
 *      supplied a refresh proc, call that.  Otherwise call its init
 *      hook again.
 */

#define JUST_INITTED(mi)	(MI_FLAG_IS_SET(mi, WI_FLAG_JUST_INITTED))

void
call_refresh_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	if (ls->refresh_hook == NULL) {
		/*
		 * No refresh hook supplied.  If the mode has been
		 * initialized, and the callback has been called at least
		 * once, then call the init hook to do the refresh.
		 * Note that two flags are examined here.  The first
		 * indicates if the mode has ever had its init hook called,
		 * the second is a per-screen flag which indicates
		 * if the draw (callback) hook has been called since the
		 * init hook was called for that screen.
		 * This second test is a hack.  A mode should gracefully
		 * deal with its init hook being called twice in a row.
		 * Once all the modes have been updated, this hack should
		 * be removed.
		 */
		if (MODE_NOT_INITED(ls) || JUST_INITTED(mi)) {
			return;
		}
		call_init_hook(ls, mi);
	} else {
		ls->refresh_hook(mi);
	}
}

/* -------------------------------------------------------------------- */

/*-
 *    The user has requested a change, by pressing the middle mouse
 *      button.  Let the mode know about it.
 */

void
call_change_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	if (ls->change_hook != NULL) {
		ls->change_hook(mi);
	}
}

/* -------------------------------------------------------------------- */
