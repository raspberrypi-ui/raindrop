/*============================================================================
Copyright (c) 2024 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define SUDO_PREFIX "env SUDO_ASKPASS=" PACKAGE_DATA_DIR "/pwdraindrop.sh sudo -A "

#define MAX_MONS 10

typedef struct {
    int width;
    int height;
    float freq;
    gboolean interlaced;
} output_mode_t;

typedef struct {
    char *name;
    gboolean enabled;
    int width;
    int height;
    int x;
    int y;
    int rotation;
    float freq;
    gboolean interlaced;
    GList *modes;
    char *touchscreen;
    char *backlight;
    gboolean primary;
} monitor_t;

typedef struct {
    void (*load_config) (void);
    void (*load_touchscreens) (void);
    void (*save_config) (void);
    void (*save_touchscreens) (void);
    void (*reload_config) (void);
    void (*reload_touchscreens) (void);
    void (*revert_config) (void);
    void (*revert_touchscreens) (void);
    void (*update_system_config) (void);
    void (*identify_monitors) (void);
} wm_functions_t;

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

extern monitor_t mons[MAX_MONS];
extern GList *touchscreens;

/* End of file */
/*============================================================================*/

