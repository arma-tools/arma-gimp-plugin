#include <aff_cxx_rust/lib.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <reader.h>
#include <rust/cxx.h>

#include <format>
#include <functional>
#include <memory>

/* The name of my PDB procedure */
#define PLUG_IN_PROC "plug-in-arma-tools-arma-gimp-plugin"

#define PROC_AGP_PAA_LOAD "plug-in-arma-tools-agp-paa-load"
#define PROC_AGP_EDDS_LOAD "plug-in-arma-tools-agp-edds-load"

#define LOAD_AGP_ERROR -1

/* Our custom class HelloWorld is derived from GimpPlugIn. */
struct _ArmaGimpPlugin {
  GimpPlugIn parent_instance;
};

#define ARMA_GIMP_PLUGIN_TYPE (arma_gimp_plugin_get_type())
G_DECLARE_FINAL_TYPE(ArmaGimpPlugin, arma_gimp_plugin, ARMA_GIMP_PLUGIN, ,
                     GimpPlugIn)

/* Declarations */
static GList* arma_gimp_plugin_query_procedures(GimpPlugIn* plug_in);
static GimpProcedure* arma_gimp_plugin_create_procedure(GimpPlugIn* plug_in,
                                                        const gchar* name);

static GimpValueArray* arma_gimp_plugin_run(
    GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
    GimpDrawable** drawables, GimpProcedureConfig* config, gpointer run_data);

G_DEFINE_TYPE(ArmaGimpPlugin, arma_gimp_plugin, GIMP_TYPE_PLUG_IN)