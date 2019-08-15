#ifndef help_text_h
#define help_text_h

/*
12345678901234567890123456789012345678901234567890123456789012345678901234567890\n\
*/
const char *help_txt = "\
Usage: %s [GLOBAL_OPTION] GROUP_DEF [-n GROUP_DEF]...\n\
\n\
Randomly chooses images from a list to draw on the X desktop background with\n\
smooth fading transitions.\n\
\n\
GLOBAL_OPTION: options that apply to this program in general\n\
\n\
  -h, --help                 prints this message\n\
  -s, --fps=FPS              number of intermediary frames per second\n\
                               (must be a non-negative float)\n\
                               default: 60.0\n\
  -d, --duration=DURATION    number of seconds the transition should take\n\
                               (must be a non-negative float)\n\
                               default: 1.0\n\
\n\
\n\
GROUP_DEF: arguments that define a group. A group consists of a list of targets,\n\
    a list of images, and a randomization method for choosing which images to\n\
    draw on the targets. For each group, use the following format:\n\
\n\
GROUP_DEF := [GROUP_OPTION]... [TARGET]... [[IMAGE_OPTION]... IMAGE]...\n\
\n\
    options specified in GROUP_OPTION will affect the current and all\n\
        subsequent groups\n\
    options specified in IMAGE_OPTION will affect all subsequent images,\n\
        including images specified in subsequent groups\n\
\n\
  -n, --new                  create a new group. Used as a group separator\n\
\n\
\n\
GROUP_OPTION: options that apply to groups\n\
\n\
  -a, --random=RANDOM        specify a randomization mode. This will determine\n\
                               how images will be chosen to get drawn on the\n\
                               targets in a group\n\
                               (RANDOM must be one of the following)\n\
                                 first:\n\
                                     use the first image in the list to draw\n\
                                     on all targets. This mode is deterministic\n\
                                 seq:\n\
                                     use the i-th image to draw on the i-th\n\
                                     target. This mode is deterministic\n\
                                 permute:\n\
                                     for each target, randomly choose an image\n\
                                     (without reuse) to draw\n\
                                 reuse:\n\
                                     for each target, randomly choose an image\n\
                                     (allowing reuse) to draw\n\
                                 choose-one:\n\
                                     randomly choose one image to draw on all\n\
                                     targets\n\
                                 permute-reuse: (default)\n\
                                     try to use the \"permute\" mode. If there's\n\
                                     not enough images, use the \"reuse\" mode.\n\
\n\
\n\
TARGET: specify a region on the X screen to draw images. Each entry will be\n\
    be added to the list of targets in the current group. If none are specified,\n\
    all XRandR CRTCs will be used as the list of target.\n\
\n\
  -g, --region=WxH+X+Y       specify a region manually by using (X,Y) as\n\
                               the top left corner and W,H as the dimensions.\n\
                               (W,H,X,Y must all integers. W,H must be positive)\n\
  -m, --monitor=MONITOR      use the XRandR output MONITOR as the region.\n\
                               (MONITOR must be a connected output)\n\
  -e, --screen               use the whole X screen as the region\n\
\n\
\n\
IMAGE_OPTION: options that apply to images. These options must be specified\n\
    before the images they are meant for\n\
\n\
  -x, --transform=XFORM      specify how images should be scaled in regards\n\
                               with their chosen targets\n\
                               (XFORM must be one of the following)\n\
                                 center:\n\
                                     center the image with no scaling\n\
                                 stretch:\n\
                                     fill the target with the image, matching\n\
                                     both width and height and ignoring the\n\
                                     aspect ratio of the image\n\
                                 match-width:\n\
                                     scale the image to match the width of the\n\
                                     target, while maintaining the aspect ratio\n\
                                 match-height:\n\
                                     scale the image to match the height of the\n\
                                     target, while maintaining the aspect ratio\n\
                                 fit:\n\
                                     scale the image to fit inside the target\n\
                                     while maintaining the aspect ratio\n\
                                 fill: (default)\n\
                                     scale the image to fill the entire target\n\
                                     while maintaining the aspect ratio\n\
  -r, --repeat=REPEAT        specifies what to do when the image is too small to\n\
                               fill the target\n\
                               (REPEAT must be one of the following)\n\
                                 none: (default)\n\
                                     no attempt will be made to fill the target\n\
                                 normal:\n\
                                     the image will be tiled to fill the target\n\
                                 pad:\n\
                                     the edges of the images will be stretched\n\
                                     to fill the target\n\
                                 reflect:\n\
                                     like \"normal\", but adjacent tiles of the\n\
                                     image are mirrored\n\
  -f, --filter=FILTER        specifies which filter to use when scaling images\n\
                               (FILTER must be one of the following)\n\
                                 nearest:\n\
                                     use nearest neighbor when scaling the image\n\
                                 bilinear: (default)\n\
                                     use bilinear interpolation when scaling the\n\
                                     image\n\
\n\
\n\
IMAGE: specify a candidate image. Each entry will be added to the list of images\n\
    in the current group. At least one image must be specified.\n\
\n\
  IMAGE_PATH                 use an image file specified by IMAGE_PATH. Supports\n\
                               a subset of JPEG, PNG, TGA, BMP, PSD, GIF, HDR,\n\
                               PIC and PNM formats.\n\
\n\
  -c, --color=COLOR          use a solid color as an image\n\
                               (COLOR must be a valid Xrender color. Some\n\
                                example formats are listed below)\n\
                                 #RRGGBB\n\
                                     where R,G,B are hexadecimal digits\n\
                                 rgba:RRRR/GGGG/BBBB/AAAA\n\
                                     where R,G,B,A are hexadecimal digits, in\n\
                                     lowercase\n\
";

#endif
