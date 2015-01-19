/* Copyright 2014-2015 Dietrich Epp.
   This file is part of SGGL.  SGGL is licensed under the terms of the
   2-clause BSD license.  For more information, see LICENSE.txt. */
#include "private.h"
#include "opengl_private.h"
#include "sg/entry.h"
#include "sggl/3_0.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#if defined _WIN32
# include <Windows.h>
#endif

/* Data defined in opengl_data.c, which is generated.  */
extern const char SG_GL_ENTRYNAME[];
#if defined SG_GL_HASEXTS
extern const char SG_GL_EXTENSIONNAME[];
#endif

int sg_glver;
#if defined SG_GL_HASEXTS
unsigned sg_glext[(SG_GL_EXTCOUNT + 31) / 32];
#endif
void *sg_glfunc[SG_GL_ENTRYCOUNT];

#if defined __APPLE__
#include <CoreFoundation/CoreFoundation.h>

void
sg_gl_getfuncs(const char *names, void **funcs, size_t count)
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
    return;

fail:
    sg_sys_abort("Could not load OpenGL.");
}

#elif defined _WIN32 || defined __linux__

# if defined _WIN32
#  define GETPROCADDRESS(name) wglGetProcAddress(name)
# elif defined __linux__
extern void (*glXGetProcAddress(const GLubyte *procname))(void);
#  define GETPROCADDRESS(name) glXGetProcAddress((const GLubyte *) name)
# endif

void
sg_gl_getfuncs(const char *names, void **funcs, size_t count)
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
}

#else
# error "Don't know how to load OpenGL functions on this platform."
#endif

#if defined SG_GL_HASEXTS

static void
sg_gl_setext(const char *ext, size_t len)
{
    const char *p;
    int i, n = SG_GL_EXTCOUNT;
    size_t elen;
    if (memcmp(ext, "GL_", 3))
        return;
    ext += 3;
    len -= 3;
    for (i = 0, p = SG_GL_EXTENSIONNAME; i < n; i++) {
        elen = strlen(p);
        if (elen == len && !memcmp(ext, p, len)) {
            sg_glext[i / 32] |= 1u << (i & 31);
            return;
        }
        p = p + elen + 1;
    }
}

#endif

void
sg_gl_init(void)
{
    const char *version;
    int major, minor;

    sg_gl_getfuncs(SG_GL_ENTRYNAME, sg_glfunc, SG_GL_ENTRYCOUNT);

    version = (const char *) glGetString(GL_VERSION);
    if (!version)
        goto bad_version;
    {
        const char *p;
        char *e;
        unsigned long x;

        x = strtoul(version, &e, 10);
        if (e == version || *e != '.' || x == 0)
            goto bad_version;
        major = (int) x;
        if (major >= 3) {
            major = minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);
            if (major < 3)
                goto bad_version;
        } else {
            p = e + 1;
            x = strtoul(p, &e, 10);
            if (e == p || (*e != ' ' && *e != '.'))
                goto bad_version;
            minor = (int) x;
        }
        if (major < 1 || major > 100) {
        bad_version:
            sg_sys_abortf("Invalid OpenGL version: %s.", version);
            return;
        }
    }
    sg_glver = major * 16 + (minor > 15 ? 15 : minor);

#if defined SG_GL_HASEXTS
    if (major < 3) {
        const char *p, *q;
        p = (const char *) glGetString(GL_EXTENSIONS);
        if (p) {
            while (1) {
                q = strchr(p, ' ');
                if (!q) {
                    sg_gl_setext(p, strlen(p));
                    break;
                }
                sg_gl_setext(p, q - p);
                p = q + 1;
            }
        }
    } else {
        int extcount = 0, i;
        const char *ext;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extcount);
        for (i = 0; i < extcount; i++) {
            ext = (const char *) glGetStringi(GL_EXTENSIONS, i);
            sg_gl_setext(ext, strlen(ext));
        }
    }
#endif

    if (sg_cvar_developer.value) {
        sg_gl_debug_init();
    }
}
