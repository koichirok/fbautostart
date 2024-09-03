/*
 * Copyright (C) 2011, Paul Tagliamonte <paultag@fluxbox.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include "exceptions.hh"

#include "main.hh"
#include "state.hh"
#include "entry.hh"
#include "machine.hh"
#include "xdg_spec.hh"
#include "xdg_parse.hh"
#include "xdg_model.hh"
#include "xdg_autostart.hh"

bool noexec = false;

/**
 * Run a given command (as if we were invoking from a shell)
 *
 * This will fork and run the `appl` command in either $SHELL (if set), or
 * /bin/sh if $SHELL is unset. This will fork to the background and return
 * control very quickly. Don't use this to do a blocking read on a process
 * or something.
 *
 * @param appl application to run
 * @return 0 (for the parent fork, which should be what you get), pid for child.
 */
int run_command( std::string appl ) {
    if ( appl == "" )
        return 0;

    pid_t pid = fork();

    if (pid)
        return pid;

    const char * shell = getenv("SHELL");

    if ( shell == NULL )
        shell = "/bin/sh";

    if ( ! noexec ) { // we'll do it live
        execl(
                shell,
                shell,
                "-c",
                appl.c_str(),
                static_cast<void*>(NULL)
             );
        exit ( EXIT_SUCCESS );
        return pid;
    } else {
        std::cout << "    Would have run: " << appl << std::endl;
        exit(0);
        return 0;
    }
}

/**
 *
 */
std::vector<std::string> split_path( std::string input ) {
    std::vector<std::string> ret;
    std::stringstream ss(input);
    std::string word;
    char delim = ':';
    while ( std::getline( ss, word, delim ) ) {
        ret.push_back(word);
    }
    return ret;
}

/**
 * pre exec routines, such as setup, setting the internal state and checks
 *
 * all of these should be safe to run more then once, and it should *always*
 * be run before allowing a stateful parse to happen.
 *
 * It'll also print out a fairly useful block of text to aid with debugging
 * problems down the road.
 */
void pre_exec() {
    const char * home       = getenv("XDG_CONFIG_HOME");
    const char * dirs       = getenv("XDG_CONFIG_DIRS");
    const char * desktopenv = getenv("FBXDG_DE");
    const char * execmodel  = getenv("FBXDG_EXEC");

    /*   TABLE OF ENV VARS
     *
     * +------------------+----------------------------+
     * | XDG_CONFIG_HOME  | XDG Local dir (~/.config)  |
     * | XDG_CONFIG_DIRS  | XDG Global dir (/etc/xdg)  |
     * | HOME             | User home (/home/tag)      |
     * | FBXDG_DE         | Who to act on behalf of    |
     * | FBXDG_EXEC       | 0 for exec, 1 for noexec   |
     * +------------------+----------------------------+
     *
     */

    const char * blank = ""; /* -Waddress errrs.
                                ( to avoid using strcmp, which we actually
                                do use later, so XXX: Fixme ) */

    home       = home       == NULL ? blank : home;
    dirs       = dirs       == NULL ? blank : dirs;
    desktopenv = desktopenv == NULL ? blank : desktopenv;
    execmodel  = execmodel  == NULL ? blank : execmodel;
    /*
     * If we're null, set to "" rather then keep NULL.
     */

    _xdg_default_global = dirs       == blank ? _xdg_default_global : dirs;
    _xdg_default_local  = home       == blank ? _xdg_default_local  : home;
    _xdg_window_manager = desktopenv == blank ? _xdg_window_manager : desktopenv;
    /*
     * If we have a env var, set them to the global prefs.
     */

    if ( strcmp(execmodel,"0") == 0 ) {
        noexec = false;
    } else if ( strcmp(execmodel,"1") == 0 ) {
        noexec = true;
    }
    /*
     * Fuck this mess. Someone needs to clean this up to use true/false
     * and have it relate in a sane way.
     * XXX: Fixme
     */

    std::cout << "" << std::endl;
    std::cout << "Hello! I'll be your friendly XDG Autostarter today." << std::endl;
    std::cout << "Here's what's on the menu:" << std::endl;
    std::cout << "" << std::endl;
    std::cout << " Appetizers" << std::endl;
    std::cout << "" << std::endl;

    std::cout << " * $XDG_CONFIG_HOME:      " << home << std::endl;
    std::cout << " * $XDG_CONFIG_DIRS:      " << dirs << std::endl;
    std::cout << " * $FBXDG_DE:             " << desktopenv << std::endl;
    std::cout << " * $FBXDG_EXEC:           " << execmodel << std::endl;

    std::cout << "" << std::endl;
    std::cout << " Entrées" << std::endl;
    std::cout << "" << std::endl;

    std::cout << " * Desktop Environment:    " << _xdg_window_manager << std::endl;
    std::cout << " * Global XDG Directory:  " << _xdg_default_global << std::endl;
    std::cout << " * Local XDG Directory:   " << _xdg_default_local  << std::endl;
    std::cout << " * Current exec Model:    " << noexec << std::endl;

    std::cout << "" << std::endl;
    std::cout << " - Chef Tagliamonte" << std::endl;
    std::cout << "       & The Fluxbox Crew" << std::endl;
    std::cout << "" << std::endl;
}

/**
 * expand tildes to the user's actual home
 *
 * @param ref string to expand
 * @param home path to user's home
 * @return either a newly expanded string or the old string
 */
std::string fix_home_pathing( std::string ref, std::string home ) {
    // std::cout << "preref: " << ref << std::endl;
    if ( ref[0] == '~' && ref[1] == '/' ) {
        // std::cout << " +-> confirmed." << std::endl;
        ref.replace(0, 1, home );
        // std::cout << " +-> postref: " << ref << std::endl;
    }
    return ref;
}

/**
 * XXX: Doc me
 */
void command_line_overrides( int argc, char ** arv ) {
    for ( int i = 1; i < argc; ++i ) {
        // process argv[i]
    }
}

int main ( int argc, char ** argv ) {
    const char * userhome   = getenv("HOME");

    pre_exec(); /* pre_exec allows us to read goodies and set up
                   the env for us. */

    xdg_autostart_map binaries; /* the map of what stuff to start
                                   up or ignore or whatever. */


    std::vector<std::string> global = split_path(_xdg_default_global);
    std::vector<std::string> local  = split_path(_xdg_default_local);

    for ( unsigned int i = 0; i < global.size(); ++i ) {
        std::string path = fix_home_pathing(global.at(i), userhome);
        parse_folder( &binaries, path + "/autostart/" );
    }
    for ( unsigned int i = 0; i < local.size(); ++i ) {
        std::string path = fix_home_pathing(local.at(i), userhome);
        parse_folder( &binaries, path + "/autostart/" );
    }

    std::cout << "" << std::endl;
    std::cout << "Finished parsing all files." << std::endl;
    std::cout << "" << std::endl;

    for ( xdg_autostart_map::iterator i = binaries.begin();
            i != binaries.end(); ++i
        ) {
        std::cout << "  Handling stub for: " << i->first << std::endl;
        run_command( i->second );
        /* foreach command, let's run it :) */
    }
}

