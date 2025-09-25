#include "main.h"

using namespace arma_file_formats::cxx;

static void arma_gimp_plugin_class_init(ArmaGimpPluginClass *klass)
{
    GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS(klass);

    plug_in_class->query_procedures = arma_gimp_plugin_query_procedures;
    plug_in_class->create_procedure = arma_gimp_plugin_create_procedure;
}

static void arma_gimp_plugin_init(ArmaGimpPlugin *arma_gimp_plugin) {}

static GList *arma_gimp_plugin_query_procedures(GimpPlugIn *plug_in)
{
    GList *list = NULL;
    list = g_list_append(list, g_strdup(PLUG_IN_PROC));
    list = g_list_append(list, g_strdup(PROC_AGP_PAA_LOAD));
    list = g_list_append(list, g_strdup(PROC_AGP_PAA_EXPORT));
    list = g_list_append(list, g_strdup(PROC_AGP_EDDS_LOAD));

    return list;
}

static const char *get_color_encoding(PixelTypeCxx &pixel_type)
{
    switch (pixel_type)
    {
    case PixelTypeCxx::Rgba:
        return "R'G'B'A u8";
        break;
    case PixelTypeCxx::Gray:
        return "Y' u8";
        break;
    case PixelTypeCxx::GrayAlpha:
        return "Y'A u8";
        break;
    default:
        break;
    }
    return "";
}

static const GimpImageBaseType get_image_base_type(PixelTypeCxx &pixel_type)
{
    if (pixel_type == PixelTypeCxx::Gray ||
        pixel_type == PixelTypeCxx::GrayAlpha)
    {
        return GIMP_GRAY;
    }
    return GIMP_RGB;
}

static const GimpImageType get_image_type(PixelTypeCxx &pixel_type)
{
    switch (pixel_type)
    {
    case PixelTypeCxx::Rgb:
        return GimpImageType::GIMP_RGB_IMAGE;
        break;

    case PixelTypeCxx::Gray:
        return GimpImageType::GIMP_GRAY_IMAGE;
        break;
    case PixelTypeCxx::GrayAlpha:
        return GimpImageType::GIMP_GRAYA_IMAGE;
        break;
    default:
        break;
    }
    return GimpImageType::GIMP_RGBA_IMAGE;
}

struct ImageData
{
    PixelTypeCxx pixel_type;
    uint16_t width;
    uint16_t height;
    ::rust::Vec<::std::uint8_t> data;
};

static ImageData load_paa(GFile *file)
{
    PaaCxx paa = load_paa_from_gfile((GFileWrapperCxx *)file, 0);
    PaaMipmapCxx mipmap = paa.mipmaps.at(0);

    return {paa.pixel_type, mipmap.width, mipmap.height, mipmap.data};
}

static ImageData load_edds(GFile *file)
{
    EddsCxx edds = load_edds_from_gfile((GFileWrapperCxx *)file, 0);
    EddsMipmapCxx mipmap = edds.mipmaps.back();

    return {edds.pixel_type, mipmap.width, mipmap.height, mipmap.data};
}

static GimpValueArray *
load_file(GimpProcedure *procedure, GimpRunMode run_mode, GFile *file,
          GimpMetadata *metadata, GimpMetadataLoadFlags *flags,
          GimpProcedureConfig *config, gpointer run_data,
          std::function<ImageData(GFile *)> load_function)
{
    GimpValueArray *return_vals;
    GError *error = NULL;

    auto filename = g_file_get_path(file);

    ImageData imageData;

    try
    {
        imageData = load_function(file);
    }
    catch (std::runtime_error &ex)
    {
        return gimp_procedure_new_return_values(
            procedure, GIMP_PDB_EXECUTION_ERROR,
            g_error_new(
                GIMP_PLUG_IN_ERROR, LOAD_AGP_ERROR, "%s",
                std::format("Exception during Import: \n %s", ex.what()).c_str()));
    }

    int width = imageData.width;
    int height = imageData.height;

    auto color_encoding = get_color_encoding(imageData.pixel_type);
    if (strcmp(color_encoding, "") == 0)
    {
        return gimp_procedure_new_return_values(
            procedure, GIMP_PDB_EXECUTION_ERROR,
            g_error_new(GIMP_PLUG_IN_ERROR, LOAD_AGP_ERROR,
                        ("Couldn't identify the color encoding!")));
    }

    GimpImage *image = gimp_image_new_with_precision(
        width, height, get_image_base_type(imageData.pixel_type),
        GIMP_PRECISION_U8_NON_LINEAR);

    GimpLayer *layer = gimp_layer_new(
        image, filename, width, height, get_image_type(imageData.pixel_type), 100,
        gimp_image_get_default_new_layer_mode(image));
    gimp_image_insert_layer(image, layer, NULL, 0);
    g_free(filename);

    GeglBuffer *buffer = gimp_drawable_get_buffer(GIMP_DRAWABLE(layer));
    gegl_buffer_set(
        buffer, GEGL_RECTANGLE(0, 0, width, height), 0,
        babl_format_with_space(color_encoding,
                               gimp_drawable_get_format(GIMP_DRAWABLE(layer))),
        imageData.data.data(), GEGL_AUTO_ROWSTRIDE);
    g_object_unref(buffer);

    return_vals =
        gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);

    GIMP_VALUES_SET_IMAGE(return_vals, 1, image);

    return return_vals;
}

static GimpValueArray *
load_edds_file(GimpProcedure *procedure, GimpRunMode run_mode, GFile *file,
               GimpMetadata *metadata, GimpMetadataLoadFlags *flags,
               GimpProcedureConfig *config, gpointer run_data)
{
    return load_file(procedure, run_mode, file, metadata, flags, config, run_data,
                     load_edds);
}

static GimpValueArray *
load_paa_file(GimpProcedure *procedure, GimpRunMode run_mode, GFile *file,
              GimpMetadata *metadata, GimpMetadataLoadFlags *flags,
              GimpProcedureConfig *config, gpointer run_data)
{
    return load_file(procedure, run_mode, file, metadata, flags, config, run_data,
                     load_paa);
}

const static bool isPowerOfTwo(uint32_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

static GimpValueArray *save_paa_file(
    GimpProcedure *procedure,
    GimpRunMode run_mode,
    GimpImage *image,
    GFile *file,
    GimpExportOptions *options,
    GimpMetadata *metadata,
    GimpProcedureConfig *config,
    gpointer run_data)
{
    auto drawables = gimp_image_list_layers(image);

    auto drawable = (GimpDrawable *)drawables->data;

    auto width = gimp_drawable_get_width(drawable);
    auto height = gimp_drawable_get_height(drawable);

    auto channelNumber = gimp_drawable_get_bpp(drawable);

    const Babl *format;
    if (channelNumber == 4)
    {
        format = babl_format("R'G'B'A u8");
    }
    else
    {
        format = babl_format("R'G'B' u8");
    }

    if (!isPowerOfTwo(width) || !isPowerOfTwo(height))
    {
        return gimp_procedure_new_return_values(
            procedure,
            GIMP_PDB_CALLING_ERROR,
            g_error_new(GIMP_PLUG_IN_ERROR, EXPORT_AGP_ERROR, "Error during Paa Export:\nDimensions have to be a power of two (2^n)"));
    }

    auto data = std::vector<uint8_t>((size_t)width * (size_t)height * channelNumber);
    GeglBuffer *buffer = gimp_drawable_get_buffer(drawable);
    gegl_buffer_get(buffer, GEGL_RECTANGLE(0, 0, width, height),
                    1, format, data.data(), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    g_object_unref(buffer);

    // Insert alpha bytes
    if (channelNumber < 4)
    {
        auto temp = std::vector<uint8_t>();
        temp.reserve((size_t)width * (size_t)height * 4);
        for (size_t i = 0; i < data.size(); i += 3)
        {
            temp.push_back(data[i]);
            temp.push_back(data[i + 1]);
            temp.push_back(data[i + 2]);
            temp.push_back(255);
        }
        data.clear();
        data = temp;
    }

    try
    {
        write_paa_to_gfile((GFileWrapperCxx *)file, data, width, height);
    }
    catch (std::runtime_error &ex)
    {
        return gimp_procedure_new_return_values(
            procedure, GIMP_PDB_EXECUTION_ERROR,
            g_error_new(
                GIMP_PLUG_IN_ERROR, EXPORT_AGP_ERROR, "%s",
                std::format("Exception during Export: \n %s", ex.what()).c_str()));
    }

    return gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);
}

static GimpProcedure *arma_gimp_plugin_create_procedure(GimpPlugIn *plug_in,
                                                        const gchar *name)
{
    GimpProcedure *procedure = NULL;

    if (g_strcmp0(name, PROC_AGP_PAA_LOAD) == 0)
    {
        procedure = gimp_load_procedure_new(
            plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, load_paa_file, NULL, NULL);

        gimp_procedure_set_sensitivity_mask(procedure,
                                            GIMP_PROCEDURE_SENSITIVE_NO_IMAGE);

        gimp_procedure_set_menu_label(procedure, "PAA image");
        gimp_procedure_set_documentation(
            procedure, "Loads files in the PAA file format",
            "This plug-in loads PAA Texture files.", NULL);
        gimp_procedure_set_attribution(procedure, "Willard", "GPL2", "2025");

        gimp_file_procedure_set_magics(GIMP_FILE_PROCEDURE(procedure),
                                       "0,leshort,65281,0,leshort,65285");
        gimp_file_procedure_set_format_name(GIMP_FILE_PROCEDURE(procedure), "PAA");
        gimp_file_procedure_set_extensions(GIMP_FILE_PROCEDURE(procedure), "paa");
    }
    else if (g_strcmp0(name, PROC_AGP_PAA_EXPORT) == 0)
    {
        procedure = gimp_export_procedure_new(plug_in, name,
                                              GIMP_PDB_PROC_TYPE_PLUGIN,
                                              TRUE, save_paa_file, NULL, NULL);

        gimp_procedure_set_image_types(procedure, "RGB*"); //, GRAY*");

        gimp_procedure_set_menu_label(procedure, "PAA image");

        gimp_procedure_set_documentation(procedure,
                                         "Exports files in the PAA file format",
                                         "This plug-in exports the image into PAA Texture files.",
                                         NULL);
        gimp_procedure_set_attribution(procedure, "Willard", "GPL2", "2025");

        gimp_file_procedure_set_format_name(GIMP_FILE_PROCEDURE(procedure),
                                            "PAA");
        gimp_file_procedure_set_extensions(GIMP_FILE_PROCEDURE(procedure),
                                           "paa");

        gimp_export_procedure_set_capabilities(GIMP_EXPORT_PROCEDURE(procedure),
                                               GIMP_EXPORT_CAN_HANDLE_RGB,
                                               //|  GIMP_EXPORT_CAN_HANDLE_GRAY,
                                               NULL,
                                               NULL, NULL);
    }
    else if (g_strcmp0(name, PROC_AGP_EDDS_LOAD) == 0)
    {
        procedure = gimp_load_procedure_new(
            plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, load_edds_file, NULL, NULL);

        gimp_procedure_set_sensitivity_mask(procedure,
                                            GIMP_PROCEDURE_SENSITIVE_NO_IMAGE);

        gimp_procedure_set_menu_label(procedure, "EDDS image");
        gimp_procedure_set_documentation(
            procedure, "Loads files in the EDDS file format",
            "This plug-in loads EDDS Texture files.", NULL);
        gimp_procedure_set_attribution(procedure, "Willard", "GPL2", "2025");

        // Conflicts with the DDS Plugin
        // gimp_file_procedure_set_magics(GIMP_FILE_PROCEDURE(procedure),
        //                                "0,string,DDS ");
        gimp_file_procedure_set_format_name(GIMP_FILE_PROCEDURE(procedure), "EDDS");
        gimp_file_procedure_set_extensions(GIMP_FILE_PROCEDURE(procedure), "edds");
        gimp_file_procedure_set_priority(GIMP_FILE_PROCEDURE(procedure), 200);
    }

    if (g_strcmp0(name, PLUG_IN_PROC) == 0)
        procedure =
            gimp_image_procedure_new(plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN,
                                     arma_gimp_plugin_run, NULL, NULL);

    return procedure;
}

static GimpValueArray *
arma_gimp_plugin_run(GimpProcedure *procedure, GimpRunMode run_mode,
                     GimpImage *image, GimpDrawable **drawables,
                     GimpProcedureConfig *config, gpointer run_data)
{
    gimp_message("Hello World!");

    return gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);
}

/* Generate needed main() code */
GIMP_MAIN(ARMA_GIMP_PLUGIN_TYPE)