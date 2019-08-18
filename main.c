#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vector.h"
#include "help_text.h"

#define INITIAL_CAPACITY 16

void print_usage(char *argv0) {
	fprintf(stderr, help_txt, argv0);
}

enum transform {
	TRANSFORM_CENTER,
	TRANSFORM_STRETCH,
	TRANSFORM_MATCH_WIDTH,
	TRANSFORM_MATCH_HEIGHT,
	TRANSFORM_FIT,
	TRANSFORM_FILL,
};

enum random_mode {
	RANDOM_FIRST,
	RANDOM_SEQ,
	RANDOM_PERMUTE,
	RANDOM_CHOOSE_ONE,
	RANDOM_REUSE,
	RANDOM_PERMUTE_REUSE,
};

enum repeat_mode {
	REPEAT_NONE,
	REPEAT_NORMAL,
	REPEAT_PAD,
	REPEAT_REFLECT,
};

enum scale_filter {
	FILTER_NEAREST,
	FILTER_BILINEAR,
};

struct target {
	int width;
	int height;
	int x;
	int y;
	int image_ind;
};

struct image_src {
	char *desc;
	void *data;
	size_t data_len;
	int width;
	int height;
	XImage *image;
	Picture picture;
	bool solid_color;
	bool loaded;
};

struct image {
	XRenderColor bgclr;
	int src_ind;
	enum transform transform;
	enum scale_filter filter;
	enum repeat_mode repeat;
};

struct target_group {
	struct vector targets; // struct target
	struct vector images; // struct image
	enum random_mode random;
};

struct monitor {
	char *monitor;
	int crtc_ind;
};

static struct option long_options[] = {
	// Global option
	{"help", no_argument, 0, 'h'},
	{"fps", required_argument, 0, 's'},
	{"duration", required_argument, 0, 'd'},

	// Group
	{"new", no_argument, 0, 'n'},
	{"random", required_argument, 0, 'a'},
	{"region", required_argument, 0, 'g'},
	{"monitor", required_argument, 0, 'm'},
	{"screen", no_argument, 0, 'e'},

	// Option
	{"transform", required_argument, 0, 'x'},
	{"repeat", required_argument, 0, 'r'},
	{"filter", required_argument, 0, 'f'},
	{"bg", required_argument, 0, 'b'},

	// Image
	{"color", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};

double fps = 60;
double duration = 1;

// struct image_src
struct vector images;
// struct target_group
struct vector groups;
// struct target
struct vector crtcs;
// struct monitor
struct vector outputs;

// Default settings
struct image image_settings = {
	.transform = TRANSFORM_FILL,
	.filter = FILTER_BILINEAR,
	.repeat = REPEAT_NONE,
};

struct target_group group_settings = {
	.random = RANDOM_PERMUTE_REUSE,
};

double min(double a, double b) {
	return a < b ? a : b;
}

double max(double a, double b) {
	return a > b ? a : b;
}

int random_fd = -1;

char *copy_str_len(char *inp, size_t len) {
	char *out = malloc(len + 1);
	memcpy(out, inp, len);
	out[len] = '\0';
	return out;
}

char *copy_str(char *inp) {
	return copy_str_len(inp, strlen(inp));
}

void finalize_group() {
	struct target_group *g = vector_getptr(&groups, groups.size - 1);
	g->random = group_settings.random;
}

void new_group() {
	struct target_group *g = vector_push(&groups, NULL);
	vector_create(&g->targets, sizeof(struct target), INITIAL_CAPACITY);
	vector_create(&g->images, sizeof(struct image), INITIAL_CAPACITY);
}

void new_target(int crtc_ind, int w, int h, int x, int y) {
	if(groups.size == 0) {
		fprintf(stderr, "no groups\n");
		exit(1);
	}
	struct target_group *g = vector_getptr(&groups, groups.size - 1);
	if(crtc_ind >= 0) {
		vector_push(&g->targets, vector_getptr(&crtcs, crtc_ind));
	} else {
		struct target *t = vector_push(&g->targets, NULL);
		t->width = w;
		t->height = h;
		t->x = x;
		t->y = y;
		t->image_ind = -1;
	}
}

void new_image(char *desc, bool solid_color) {
	if(groups.size == 0) {
		fprintf(stderr, "no groups\n");
		exit(1);
	}

	struct target_group *g = vector_getptr(&groups, groups.size - 1);

	int img_ind = -1;

	for(int i = 0; i < images.size; ++i) {
		struct image_src *imgsrc = vector_getptr(&images, i);
		if(imgsrc->solid_color == solid_color && strcmp(imgsrc->desc, desc) == 0) {
			img_ind = i;
		}
	}

	if(img_ind == -1) {
		struct image_src *imgsrc = vector_push(&images, NULL);
		imgsrc->desc = copy_str(desc);
		imgsrc->solid_color = solid_color;
		img_ind = images.size - 1;
	}

	struct image *img = vector_push(&g->images, NULL);
	img->src_ind = img_ind;
	img->transform = image_settings.transform;
	img->filter = image_settings.filter;
	img->repeat = image_settings.repeat;
	img->bgclr = image_settings.bgclr;
}

int add_crtc(int w, int h, int x, int y) {
	if(w == 0 || h == 0) {
		return -1;
	}
	for(size_t i = 0; i < crtcs.size; ++i) {
		struct target *t = vector_getptr(&crtcs, i);
		if(t->width == w && t->height == h && t->x == x && t->y == y) {
			return i;
		}
	}
	
	struct target *t = vector_push(&crtcs, NULL);
	
	t->width = w;
	t->height = h;
	t->x = x;
	t->y = y;
	t->image_ind = -1;

	return crtcs.size - 1;
}

int find_output(char *name, int name_len) {
	for(size_t i = 0; i < outputs.size; ++i) {
		struct monitor *m = vector_getptr(&outputs, i);
		if(strncmp(m->monitor, name, name_len) == 0 && m->monitor[name_len] == '\0') {
			return i;
		}
	}
	return -1;
}

int add_output(char *name, int name_len, int crtc_ind) {
	int find_ind = find_output(name, name_len);
	if(find_ind >= 0) {
		return find_ind;
	}

	struct monitor *m = vector_push(&outputs, NULL);
	m->monitor = copy_str_len(name, name_len);
	m->crtc_ind = crtc_ind;

	return outputs.size - 1;
}

uint32_t get_random(uint32_t bound) {
	if(random_fd == -1) {
		random_fd = open("/dev/urandom", O_RDONLY);
		if(random_fd == -1) {
			perror("open /dev/urandom");
			exit(1);
		}
	}
	uint64_t bytes;
	if(read(random_fd, &bytes, 8) != 8) {
		perror("read /dev/urandom");
		exit(1);
	}
	return (uint32_t) (bytes / 18446744073709551616.0 * bound);
}

void close_random() {
	if(random_fd != -1) {
		close(random_fd);
	}
}

void shuffle(struct vector *v) {
	for(int i = 1; i < v->size; ++i) {
		vector_swap(v, i, get_random(i + 1));
	}
}

void parse_color(Display *display, char *clr, XRenderColor *out) {
	if(!XRenderParseColor(display, clr, out)) {
		fprintf(stderr, "failed to parse color: %s\n", clr);
		exit(1);
	}
}

void load_image(Display *display, Drawable drawable, Visual *visual, struct image *img) {
	struct image_src *src = vector_getptr(&images, img->src_ind);

	int n;

	if(!src->loaded) {
		if(src->solid_color) {
			XRenderColor color;
			parse_color(display, src->desc, &color);

			src->picture = XRenderCreateSolidFill(display, &color);
			src->width = src->height = 1;
		} else {
			src->data = stbi_load(src->desc, &src->width, &src->height, &n, 4);
			if(src->data == NULL) {
				fprintf(stderr, "image failed to load: %s\n", src->desc);
				exit(1);
			}

			char *data = (char *) src->data;

			for(size_t y = 0; y < src->height; ++y) {
				for(size_t x = 0; x < src->width; ++x) {
					size_t ind = (x + y * src->width) * 4;
					unsigned char tmp = data[ind];
					data[ind] = data[ind + 2];
					data[ind + 2] = tmp;
				}
			}

			src->image = XCreateImage(display, visual, 32, ZPixmap, 0, (char *) src->data, src->width, src->height, 32, src->width * 4);
			if(!src->image) {
				fprintf(stderr, "XCreateImage failed\n");
				exit(1);
			}

			Pixmap src_pixmap = XCreatePixmap(display, drawable, src->width, src->height, 32);
			GC src_gc = XCreateGC(display, src_pixmap, 0, NULL);
			XPutImage(display, src_pixmap, src_gc, src->image, 0, 0, 0, 0, src->width, src->height);
			XFreeGC(display, src_gc);

			XRenderPictFormat *src_format = XRenderFindStandardFormat(display, PictStandardARGB32);
			src->picture = XRenderCreatePicture(display, src_pixmap, src_format, 0, NULL);

			XFreePixmap(display, src_pixmap);
		}
		src->loaded = true;
	}
}

int main(int argc, char *argv[]) {
	if(argc == 1) {
		print_usage(argv[0]);
		return 1;
	}

	// Init data structures
	vector_create(&images, sizeof(struct image_src), INITIAL_CAPACITY);
	vector_create(&groups, sizeof(struct target_group), INITIAL_CAPACITY);
	vector_create(&crtcs, sizeof(struct target), INITIAL_CAPACITY);
	vector_create(&outputs, sizeof(struct monitor), INITIAL_CAPACITY);

	// Setup
	Display *display = XOpenDisplay(NULL);
	if(display == NULL) {
		fprintf(stderr, "XOpenDisplay failed\n");
		return 1;
	}

	Window root = DefaultRootWindow(display);
	int screen = DefaultScreen(display);

	XVisualInfo visual_info;
	Visual *visual;

	if(!XMatchVisualInfo(display, screen, 24, DirectColor, &visual_info)) {
		fprintf(stderr, "XMatchVisualInfo failed\n");
		return 1;
	}

	visual = visual_info.visual;

	// Query monitors
	XRRScreenResources *xrr_screen = XRRGetScreenResourcesCurrent(display, root);
	for(int i = 0; i < xrr_screen->ncrtc; ++i) {
		XRRCrtcInfo *xrr_crtc = XRRGetCrtcInfo(display, xrr_screen, xrr_screen->crtcs[i]);
		int crtc_ind = add_crtc(xrr_crtc->width, xrr_crtc->height, xrr_crtc->x, xrr_crtc->y);
		for(int j = 0; j < xrr_crtc->noutput; ++j) {
			XRROutputInfo *xrr_output = XRRGetOutputInfo(display, xrr_screen, xrr_crtc->outputs[j]);
			add_output(xrr_output->name, xrr_output->nameLen, crtc_ind);
			XRRFreeOutputInfo(xrr_output);
		}
		XRRFreeCrtcInfo(xrr_crtc);
	}
	XRRFreeScreenResources(xrr_screen);

	// Get screen size
	XWindowAttributes root_attr;
	XGetWindowAttributes(display, root, &root_attr);

	int root_width = root_attr.width;
	int root_height = root_attr.height;

	// Argument parsing
	new_group();

	int x, y, w, h, output_ind;

	while(1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "-hs:vd:ng:m:ex:r:a:f:b:c:", long_options, &option_index);
		if(c == -1) {
			break;
		}
		switch(c) {
			case 1:
				new_image(optarg, false);
				break;
			case 's':
				if(sscanf(optarg, " %lf", &fps) != 1 || fps < 0) {
					fprintf(stderr, "--fps/-s requires a non-negative float, got: %s\n", optarg);
					return 1;
				}
				break;
			case 'd':
				if(sscanf(optarg, " %lf", &duration) != 1 || duration < 0) {
					fprintf(stderr, "--duration/-d requires a non-negative float, got: %s\n", optarg);
					return 1;
				}
				break;
			case 'n':
				finalize_group();
				new_group();
				break;
			case 'g':
				if(sscanf(optarg, " %dx%d%d%d", &w, &h, &x, &y) != 4 || w <= 0 || h <= 0) {
					fprintf(stderr, "--region/-g requires integers with width and height positive, got %s\n", optarg);
					return 1;
				}
				new_target(-1, w, h, x, y);
				break;
			case 'm':
				output_ind = find_output(optarg, strlen(optarg));
				if(output_ind == -1) {
					fprintf(stderr, "--monitor/-m requires a connected output, got %s\n", optarg);
					return 1;
				}
				new_target(((struct monitor *) vector_getptr(&outputs, output_ind))->crtc_ind, 0, 0, 0, 0);
				break;
			case 'e':
				new_target(-1, root_width, root_height, 0, 0);
				break;
			case 'x':
				if(strcmp(optarg, "center") == 0)
					image_settings.transform = TRANSFORM_CENTER;
				else if(strcmp(optarg, "stretch") == 0)
					image_settings.transform = TRANSFORM_STRETCH;
				else if(strcmp(optarg, "match-width") == 0)
					image_settings.transform = TRANSFORM_MATCH_WIDTH;
				else if(strcmp(optarg, "match-height") == 0)
					image_settings.transform = TRANSFORM_MATCH_HEIGHT;
				else if(strcmp(optarg, "fit") == 0)
					image_settings.transform = TRANSFORM_FIT;
				else if(strcmp(optarg, "fill") == 0)
					image_settings.transform = TRANSFORM_FILL;
				else {
					fprintf(stderr, "--transform/-x requires one of [\"center\", \"stretch\", \"match-width\", \"match-height\", \"fit\", \"fill\"], got %s\n", optarg);
					return 1;
				}
				break;
			case 'r':
				if(strcmp(optarg, "none") == 0) {
					image_settings.repeat = REPEAT_NONE;
				} else if(strcmp(optarg, "normal") == 0) {
					image_settings.repeat = REPEAT_NORMAL;
				} else if(strcmp(optarg, "pad") == 0) {
					image_settings.repeat = REPEAT_PAD;
				} else if(strcmp(optarg, "reflect") == 0) {
					image_settings.repeat = REPEAT_REFLECT;
				} else {
					fprintf(stderr, "--repeat/-r requires one of [\"none\", \"normal\", \"pad\", \"reflect\"], got %s\n", optarg);
					return 1;
				}
				break;
			case 'a':
				if(strcmp(optarg, "first") == 0)
					group_settings.random = RANDOM_FIRST;
				else if(strcmp(optarg, "seq") == 0)
					group_settings.random = RANDOM_SEQ;
				else if(strcmp(optarg, "permute") == 0)
					group_settings.random = RANDOM_PERMUTE;
				else if(strcmp(optarg, "choose-one") == 0)
					group_settings.random = RANDOM_CHOOSE_ONE;
				else if(strcmp(optarg, "reuse") == 0)
					group_settings.random = RANDOM_REUSE;
				else if(strcmp(optarg, "permute-reuse") == 0)
					group_settings.random = RANDOM_PERMUTE_REUSE;
				else {
					fprintf(stderr, "--random/-a requires one of \"first\", \"seq\", \"permute\", \"choose-one\", \"reuse\", \"permute-reuse\", got %s\n", optarg);
					return 1;
				}
				break;
			case 'f':
				if(strcmp(optarg, "bilinear") == 0) {
					image_settings.filter = FILTER_BILINEAR;
				} else if(strcmp(optarg, "nearest") == 0) {
					image_settings.filter = FILTER_NEAREST;
				} else {
					fprintf(stderr, "--filter/-f requires one of [\"bilinear\", \"nearest\"], got %s\n", optarg);
					return 1;
				}
				break;
			case 'b':
				parse_color(display, optarg, &image_settings.bgclr);
				break;
			case 'c':
				new_image(optarg, true);
				break;
			case '?':
				fprintf(stderr, "\n");
			case 'h':
				print_usage(argv[0]);
				return 1;
		}
	}

	finalize_group();

	// Image assignment
	for(int i = 0; i < groups.size; ++i) {
		struct target_group *g = vector_getptr(&groups, i);
		if(g->targets.size == 0) {
			for(int j = 0; j < crtcs.size; ++j) {
				vector_push(&g->targets, vector_getptr(&crtcs, j));
			}
		}

		if(g->images.size == 0 && g->targets.size != 0) {
			fprintf(stderr, "no images to draw on %zu targets\n", g->targets.size);
			return 1;
		}

		if(g->random == RANDOM_PERMUTE || g->random == RANDOM_SEQ) {
			if(g->images.size < g->targets.size) {
				fprintf(stderr, "not enough images (%zu) to draw on %zu targets\n", g->images.size, g->targets.size);
				return 1;
			}
		}

		if(g->random == RANDOM_PERMUTE_REUSE) {
			if(g->images.size < g->targets.size) {
				g->random = RANDOM_REUSE;
			} else {
				g->random = RANDOM_PERMUTE;
			}
		}

		if(g->random == RANDOM_PERMUTE || g->random == RANDOM_CHOOSE_ONE) {
			shuffle(&g->images);
		}

		for(int j = 0; j < g->targets.size; ++j) {
			struct target *t = vector_getptr(&g->targets, j);
			switch(g->random) {
				case RANDOM_FIRST:
				case RANDOM_CHOOSE_ONE:
					t->image_ind = 0;
					break;
				case RANDOM_SEQ:
				case RANDOM_PERMUTE:
					t->image_ind = j;
					break;
				case RANDOM_REUSE:
					t->image_ind = get_random(g->images.size);
					break;
				default:
					t->image_ind = -1;
			}
		}
	}

	// Get previous background
	Atom prop_rootpmap = XInternAtom(display, "_XROOTPMAP_ID", True);
	Atom prop_esetroot = XInternAtom(display, "ESETROOT_PMAP_ID", True);

	Pixmap pre_pixmap = None;

	Atom type;
	int format;
	unsigned long length, bytes_after;
	unsigned char *data_prop = 0;

	if(prop_rootpmap != None && XGetWindowProperty(display, root, prop_rootpmap, 0, 1, False, AnyPropertyType, &type, &format, &length, &bytes_after, &data_prop) == Success) {
		if(type == XA_PIXMAP) {
			pre_pixmap = *(Pixmap *) data_prop;
		}
		XFree(data_prop);
	}

	if(prop_esetroot != None && XGetWindowProperty(display, root, prop_esetroot, 0, 1, False, AnyPropertyType, &type, &format, &length, &bytes_after, &data_prop) == Success) {
		if(type == XA_PIXMAP) {
			if(pre_pixmap == None) {
				pre_pixmap = *(Pixmap *) data_prop;
			}
		}
		XFree(data_prop);
	}

	// Source pixmap - the images to be drawn
	Pixmap src_pixmap = XCreatePixmap(display, root, root_width, root_height, 24);

	// Destination pixmap - the desktop
	Pixmap dst_pixmap = XCreatePixmap(display, root, root_width, root_height, 24);

	// Xrender
	XRenderPictFormat *xrender_format = XRenderFindStandardFormat(display, PictStandardRGB24);

	Picture pre_picture;
	if(pre_pixmap == None) {
		XRenderColor color = {};
		pre_picture = XRenderCreateSolidFill(display, &color);
	} else {
		XRenderPictureAttributes attr = {
			.repeat = 1, // repeat previous desktop background, for display reconfigurations
		};
		pre_picture = XRenderCreatePicture(display, pre_pixmap, xrender_format, CPRepeat, &attr);
	}
	Picture src_picture = XRenderCreatePicture(display, src_pixmap, xrender_format, 0, NULL);
	Picture dst_picture = XRenderCreatePicture(display, dst_pixmap, xrender_format, 0, NULL);

	XRenderComposite(display, PictOpSrc, pre_picture, None, src_picture, 0, 0, 0, 0, 0, 0, root_width, root_height);

	// Prepare rendering
	prop_rootpmap = XInternAtom(display, "_XROOTPMAP_ID", False);
	prop_esetroot = XInternAtom(display, "ESETROOT_PMAP_ID", False);

	XSetCloseDownMode(display, RetainPermanent);

	// Load and render images to src_picture
	for(int i = 0; i < groups.size; ++i) {
		struct target_group *tg = vector_getptr(&groups, i);
		for(int j = 0; j < tg->targets.size; ++j) {
			struct target *t = vector_getptr(&tg->targets, j);
			struct image *img = vector_getptr(&tg->images, t->image_ind);
			load_image(display, root, visual, img);
			struct image_src *src = vector_getptr(&images, img->src_ind);

			XRenderPictureAttributes src_attr;
			switch(img->repeat) {
				case REPEAT_NONE:
					src_attr.repeat = RepeatNone;
					break;
				case REPEAT_NORMAL:
					src_attr.repeat = RepeatNormal;
					break;
				case REPEAT_PAD:
					src_attr.repeat = RepeatPad;
					break;
				case REPEAT_REFLECT:
					src_attr.repeat = RepeatReflect;
					break;
			}

			XRenderChangePicture(display, src->picture, CPRepeat, &src_attr);

			double sx = 1;
			double sy = 1;

			double ratio_width = (double) t->width / src->width;
			double ratio_height = (double) t->height / src->height;

			switch(img->transform) {
				case TRANSFORM_CENTER:
					break;
				case TRANSFORM_STRETCH:
					sx = ratio_width;
					sy = ratio_height;
					break;
				case TRANSFORM_MATCH_WIDTH:
					sx = sy = ratio_width;
					break;
				case TRANSFORM_MATCH_HEIGHT:
					sx = sy = ratio_height;
					break;
				case TRANSFORM_FIT:
					sx = sy = min(ratio_width, ratio_height);
					break;
				case TRANSFORM_FILL:
					sx = sy = max(ratio_width, ratio_height);
					break;
			}

			double tx = (t->width - src->width * sx) / 2.0;
			double ty = (t->height - src->height * sy) / 2.0;

			XTransform transform = {{
				{XDoubleToFixed(1.0 / sx), XDoubleToFixed(0.0), XDoubleToFixed(-tx / sx)},
				{XDoubleToFixed(0.0), XDoubleToFixed(1.0 / sy), XDoubleToFixed(-ty / sx)},
				{XDoubleToFixed(0.0), XDoubleToFixed(0.0), XDoubleToFixed(1.0)},
			}};
			XRenderSetPictureTransform(display, src->picture, &transform);

			const char *filter = FilterBilinear;

			switch(img->filter) {
				case FILTER_NEAREST:
					filter = FilterNearest;
					break;
				case FILTER_BILINEAR:
					filter = FilterBilinear;
					break;
			}

			XRenderSetPictureFilter(display, src->picture, filter, 0, 0);

			XRenderFillRectangle(display, PictOpOver, src_picture, &img->bgclr, t->x, t->y, t->width, t->height);

			XRenderComposite(display, PictOpOver, src->picture, None, src_picture, 0, 0, 0, 0, t->x, t->y, t->width, t->height);
		}
	}

	double alpha = 0;

	int steps = fps * duration;

	// Start rendering
	for(uint64_t step = 0; step <= steps; ++step) {
	
		if(step != 0) {
			usleep(duration * 1000000 / steps);
		}

		alpha = (double) step / steps;

		// Alpha mask
		XRenderColor alpha_mask_clr = {
			.red = 0xffff,
			.green = 0xffff,
			.blue = 0xffff,
			.alpha = (unsigned short) (min(alpha, 1) * 0xffff),
		};

		Picture alpha_mask = XRenderCreateSolidFill(display, &alpha_mask_clr);

		XRenderComposite(display, PictOpOver, pre_picture, None, dst_picture, 0, 0, 0, 0, 0, 0, root_width, root_height);
		XRenderComposite(display, PictOpOver, src_picture, alpha_mask, dst_picture, 0, 0, 0, 0, 0, 0, root_width, root_height);

		XSetWindowBackgroundPixmap(display, root, dst_pixmap);
		XChangeProperty(display, root, prop_rootpmap, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &dst_pixmap, 1);
		XChangeProperty(display, root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &dst_pixmap, 1);

		XClearWindow(display, root);
		XFlush(display);

		XRenderFreePicture(display, alpha_mask);
	}

	// Cleanup
	XRenderFreePicture(display, pre_picture);
	XRenderFreePicture(display, src_picture);
	XRenderFreePicture(display, dst_picture);

	XKillClient(display, pre_pixmap);
	XFreePixmap(display, src_pixmap);

	for(int i = 0; i < images.size; ++i) {
		struct image_src *src = vector_getptr(&images, i);
		free(src->desc);
		if(src->loaded) {
			XRenderFreePicture(display, src->picture);
			if(!src->solid_color) {
				XFree(src->image);
				stbi_image_free(src->data);
			}
		}
	}
	vector_destroy(&images);

	for(int i = 0; i < groups.size; ++i) {
		struct target_group *tg = vector_getptr(&groups, i);
		vector_destroy(&tg->targets);
		vector_destroy(&tg->images);
	}
	vector_destroy(&groups);

	for(int i = 0; i < outputs.size; ++i) {
		struct monitor *m = vector_getptr(&outputs, i);
		free(m->monitor);
	}
	vector_destroy(&outputs);
	vector_destroy(&crtcs);

	close_random();

	XCloseDisplay(display);

	return 0;
}
