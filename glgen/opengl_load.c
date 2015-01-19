/* Copyright 2014-2015 Dietrich Epp.
   This file is part of SGGL.  SGGL is licensed under the terms of the
   2-clause BSD license.  For more information, see LICENSE.txt. */
#include "sggl/3_0.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#if defined _WIN32
# include <Windows.h>
#endif

/* Data defined in opengl_data.c, which is generated.  */
extern const char SGGL_ENTRYNAME[];
#if defined SGGL_HASEXTS
extern const char SGGL_EXTENSIONNAME[];
#endif

int sggl_ver;
#if defined SGGL_HASEXTS
unsigned sggl_ext[(SGGL_EXTCOUNT + 31) / 32];
#endif
void *sggl_func[SGGL_ENTRYCOUNT];

#if defined __APPLE__
#include <CoreFoundation/CoreFoundation.h>

int
sggl_getfuncs(const char *names, void **funcs, size_t count)
{
    CFBundleRef bundle;
    CFStringRef str;
    CFArrayRef arr = NULL;
    CFArrayCallBacks acb;
    const void **cfnames = NULL;
    char sname[128];
    const char *p;
    size_t i, len;

    bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (!bundle)
        goto fail;

    sname[0] = 'g';
    sname[1] = 'l';
    cfnames = calloc(count, sizeof(*cfnames));
    if (!cfnames)
        goto fail;
    for (p = names, i = 0; i < count; i++) {
        len = strlen(p);
        assert(len < sizeof(sname) - 2);
        memcpy(sname + 2, p, len);
        p = p + len + 1;

        str = CFStringCreateWithBytes(
            kCFAllocatorDefault, (const UInt8 *) sname, len + 2,
            kCFStringEncodingASCII, false);
        if (!str)
            goto fail;
        cfnames[i] = str;
    }

    acb.version = 0;
    acb.retain = NULL;
    acb.release = NULL;
    acb.copyDescription = NULL;
    acb.equal = NULL;
    arr = CFArrayCreate(kCFAllocatorDefault, cfnames, count, &acb);
    if (!arr)
        goto fail;

    CFBundleGetFunctionPointersForNames(bundle, arr, funcs);

    CFRelease(arr);
    for (i = 0; i < count; i++)
        CFRelease(cfnames[i]);
    free(cfnames);
    CFRelease(bundle);
    return 0;

fail:
    return -1;
}

#elif defined _WIN32 || defined __linux__

# if defined _WIN32
#  define GETPROCADDRESS(name) wglGetProcAddress(name)
# elif defined __linux__
extern void (*glXGetProcAddress(const GLubyte *procname))(void);
#  define GETPROCADDRESS(name) glXGetProcAddress((const GLubyte *) name)
# endif

int
sggl_getfuncs(const char *names, void **funcs, size_t count)
{
    char sname[128];
    size_t i, len;
    const char *p;

    sname[0] = 'g';
    sname[1] = 'l';
    for (p = names, i = 0; i < count; i++) {
        len = strlen(p);
        assert(len < sizeof(sname) - 2);
        memcpy(sname + 2, p, len + 1);
        p = p + len + 1;
        funcs[i] = GETPROCADDRESS(sname);
    }

    return 0;
}

#else
# error "Don't know how to load OpenGL functions on this platform."
#endif

#if defined SGGL_HASEXTS

static void
sggl_setext(const char *ext, size_t len)
{
    const char *p;
    int i, n = SGGL_EXTCOUNT;
    size_t elen;
    if (memcmp(ext, "GL_", 3))
        return;
    ext += 3;
    len -= 3;
    for (i = 0, p = SGGL_EXTENSIONNAME; i < n; i++) {
        elen = strlen(p);
        if (elen == len && !memcmp(ext, p, len)) {
            sggl_ext[i / 32] |= 1u << (i & 31);
            return;
        }
        p = p + elen + 1;
    }
}

#endif

int
sggl_init(void)
{
    const char *version;
    int major, minor;

    if (sggl_getfuncs(SGGL_ENTRYNAME, sggl_func, SGGL_ENTRYCOUNT))
        return -1;

    version = (const char *) glGetString(GL_VERSION);
    if (!version)
        return -1;

    {
        const char *p;
        char *e;
        unsigned long x;

        x = strtoul(version, &e, 10);
        if (e == version || *e != '.' || x == 0)
            return -1;
        major = (int) x;
        if (major >= 3) {
            major = minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);
            if (major < 3)
                return -1;
        } else {
            p = e + 1;
            x = strtoul(p, &e, 10);
            if (e == p || (*e != ' ' && *e != '.'))
                return -1;
            minor = (int) x;
        }
        if (major < 1 || major > 100)
            return -1;
    }
    sggl_ver = major * 16 + (minor > 15 ? 15 : minor);

#if defined SGGL_HASEXTS
    if (major < 3) {
        const char *p, *q;
        p = (const char *) glGetString(GL_EXTENSIONS);
        if (p) {
            while (1) {
                q = strchr(p, ' ');
                if (!q) {
                    sggl_setext(p, strlen(p));
                    break;
                }
                sggl_setext(p, q - p);
                p = q + 1;
            }
        }
    } else {
        int extcount = 0, i;
        const char *ext;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extcount);
        for (i = 0; i < extcount; i++) {
            ext = (const char *) glGetStringi(GL_EXTENSIONS, i);
            sggl_setext(ext, strlen(ext));
        }
    }
#endif

    return 0;
}
