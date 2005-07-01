/*
 * This has been derived from the implementation in the FreeBSD libc.
 *
 * 2000-12-28  Ha*vard Kva*len <havardk@xmms.org>:
 * Stripped down to only mkdtemp() and made more portable
 * 
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const unsigned char padchar[] =
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

char * mkdtemp(char *path)
{
        register char *start, *trv, *suffp;
        char *pad;
        struct stat sbuf;
        int rval;

        for (trv = path; *trv; ++trv)
                ;
        suffp = trv;
        --trv;
        if (trv < path) {
                errno = EINVAL;
                return NULL;
        }

        /* Fill space with random characters */
        /*
         * I hope this is random enough.  The orginal implementation
         * uses arc4random(3) which is not available everywhere.
         */
        while (*trv == 'X') {
                int randv = random() % (sizeof(padchar) - 1);
                *trv-- = padchar[randv];
        }
        start = trv + 1;

        /*
         * check the target directory.
         */
        for (;; --trv) {
                if (trv <= path)
                        break;
                if (*trv == '/') {
                        *trv = '\0';
                        rval = stat(path, &sbuf);
                        *trv = '/';
                        if (rval != 0)
                                return NULL;
                        if (!S_ISDIR(sbuf.st_mode)) {
                                errno = ENOTDIR;
                                return NULL;
                        }
                        break;
                }
        }

        for (;;) {
                if (mkdir(path, 0700) == 0)
                        return path;
                if (errno != EEXIST)
                        return NULL;

                /* If we have a collision, cycle through the space of filenames */
                for (trv = start;;) {
                        if (*trv == '\0' || trv == suffp)
                                return NULL;
                        pad = strchr(padchar, *trv);
                        if (pad == NULL || !*++pad)
                                *trv++ = padchar[0];
                        else {
                                *trv++ = *pad;
                                break;
                        }
                }
        }
        /*NOTREACHED*/
}
