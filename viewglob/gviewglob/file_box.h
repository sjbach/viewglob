
#ifndef FILE_BOX_H
#define FILE_BOX_H

//#include <gtk/gtkcontainer.h>
#include <gtk/gtk.h>
#include <wrap_box.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define FILE_BOX_TYPE            (file_box_get_type())
#define FILE_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST( (obj), FILE_BOX_TYPE, FileBox))
#define FILE_BOX_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST( (klass), FILE_BOX_TYPE, FileBoxClass))
#define IS_FILE_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE( (obj), FILE_BOX_TYPE))
#define IS_FILE_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass), FILE_BOX_TYPE))
#define FILE_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS( (obj), FILE_BOX_TYPE, FileBoxClass))

/* --- typedefs --- */
typedef struct _FileBox FileBox;
typedef struct _FileBoxClass FileBoxClass;
typedef enum _FileBoxOrdering FileBoxOrdering;
typedef struct _FItem FItem;
typedef enum _FileType FileType;
typedef enum _FileSelection FileSelection;

/* --- enumerations --- */
enum _FileBoxOrdering {
	FBO_LS,
	FBO_WIN,
};

enum _FileType {
	FT_FILE,
	FT_DIR,
};

enum _FileSelection {
	FS_YES = GTK_STATE_SELECTED,
	FS_NO = GTK_STATE_NORMAL,
	FS_MAYBE = GTK_STATE_ACTIVE,
};

/* --- FileBox --- */
struct _FileBox {
	WrapBox   wbox;

	guint     optimal_width;
	gboolean  show_hidden_files;
	guint     file_display_limit;

	guint         n_files;
	guint         n_displayed_files;
	guint         file_max;
	//GCompareFunc  sort_func;

	GSList*   fitems;
};

struct _FileBoxClass {
	WrapBoxClass parent_class;
	//GtkContainerClass parent_class;
};

struct _FItem {
	GtkWidget*     widget;
	GString*       name;
	FileType       type;
	FileSelection  selection;
	gboolean       displayed;
	gboolean       marked;
	//FItem*         next;
};


/* --- prototypes --- */
GType       file_box_get_type(void) G_GNUC_CONST;
GtkWidget*  file_box_new(void);
void        file_box_set_optimal_width(FileBox* fbox, guint optimal_width);
void        file_box_set_show_hidden_files(FileBox* fbox, gboolean show);
void        file_box_set_file_display_limit(FileBox* fbox, guint limit);
void        file_box_set_ordering(FileBoxOrdering fbo);
void        file_box_set_icon(FileType type, GdkPixbuf* icon);
void        file_box_add(FileBox* fbox, GString* name, FileType type, FileSelection selection);
void        file_box_unmark_all(FileBox* fbox);
void        file_box_cull(FileBox* fbox);


G_END_DECLS

#endif  /* !FILE_BOX_H */

