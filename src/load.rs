use arma_file_formats::{core::types::PixelType, enfusion::edds::Edds, real_virtuality::paa::Paa};
use gimp::prelude::*;

use crate::error;

/// The decoded top level mipmap of a texture.
pub struct ImageData {
    pixel_type: PixelType,
    width: u32,
    height: u32,
    data: Vec<u8>,
}

/// Babl encoding, image base type and layer type of the pixel data.
fn pixel_format(
    pixel_type: &PixelType,
) -> Option<(&'static str, gimp::ImageBaseType, gimp::ImageType)> {
    use gimp::{ImageBaseType as Base, ImageType as Type};

    match pixel_type {
        PixelType::Rgba => Some(("R'G'B'A u8", Base::Rgb, Type::RgbaImage)),
        PixelType::Rgb => Some(("R'G'B' u8", Base::Rgb, Type::RgbImage)),
        PixelType::Gray => Some(("Y' u8", Base::Gray, Type::GrayImage)),
        PixelType::GrayAlpha => Some(("Y'A u8", Base::Gray, Type::GrayaImage)),
        PixelType::Unknown => None,
    }
}

pub fn load_paa(file: &gio::File) -> Result<ImageData, glib::Error> {
    let mut stream = file.read(gio::Cancellable::NONE)?.into_read();
    // Only decode the first, full size mipmap.
    let paa = Paa::from_reader(&mut stream, Some(&[0]))
        .map_err(|e| error(format!("Exception during Import: \n {e}")))?;
    let mipmap = paa
        .mipmaps
        .into_iter()
        .next()
        .ok_or_else(|| error("The PAA file contains no mipmaps"))?;

    Ok(ImageData {
        pixel_type: paa.pixel_type,
        width: mipmap.width.into(),
        height: mipmap.height.into(),
        data: mipmap.data,
    })
}

pub fn load_edds(file: &gio::File) -> Result<ImageData, glib::Error> {
    let mut stream = file.read(gio::Cancellable::NONE)?.into_read();
    let mut edds =
        Edds::from(&mut stream).map_err(|e| error(format!("Exception during Import: \n {e}")))?;
    // EDDS mipmaps are stored smallest first.
    let mipmap = edds
        .mipmaps
        .pop()
        .ok_or_else(|| error("The EDDS file contains no mipmaps"))?;

    Ok(ImageData {
        pixel_type: edds.pixel_type,
        width: mipmap.width.try_into().map_err(error)?,
        height: mipmap.height.try_into().map_err(error)?,
        data: mipmap.data,
    })
}

fn create_image(file: &gio::File, image_data: ImageData) -> Result<gimp::Image, glib::Error> {
    let ImageData {
        pixel_type,
        width,
        height,
        data,
    } = image_data;
    let (encoding, base_type, layer_type) =
        pixel_format(&pixel_type).ok_or_else(|| error("Couldn't identify the color encoding!"))?;

    let format = gimp::babl::Format::from_encoding(encoding);
    let expected_len = width as usize * height as usize * format.bytes_per_pixel() as usize;
    if data.len() != expected_len {
        return Err(error(format!(
            "Decoded {} bytes for a {width}x{height} {encoding} image, expected {expected_len}",
            data.len()
        )));
    }

    let (w, h) = (
        i32::try_from(width).map_err(error)?,
        i32::try_from(height).map_err(error)?,
    );
    let image = gimp::Image::with_precision(w, h, base_type, gimp::Precision::U8NonLinear);
    let name = file.basename().map(|path| path.display().to_string());
    let layer = gimp::Layer::new(
        &image,
        name.as_deref(),
        w,
        h,
        layer_type,
        100.0,
        image.default_new_layer_mode(),
    );
    image.insert_layer(&layer, gimp::Layer::NONE, 0);

    let buffer = layer.buffer().ok_or_else(|| error("The layer has no buffer"))?;
    let format = gimp::babl::Format::format_with_space(encoding, &layer.format().space());
    buffer.set_pixels(
        &gimp::gegl::Rectangle::new(0, 0, width, height),
        0,
        &format,
        &data,
        None,
    );
    buffer.flush();

    Ok(image)
}

/// Run function of the load procedures.
pub fn run(
    procedure: &gimp::LoadProcedure,
    file: &gio::File,
    load: fn(&gio::File) -> Result<ImageData, glib::Error>,
) -> gimp::ValueArray {
    procedure.return_result(load(file).and_then(|data| create_image(file, data)).and_then(
        |image| {
            let values = procedure.new_return_values(gimp::PDBStatusType::Success, None);
            values.set(1, &image).map_err(error)?;
            Ok(values)
        },
    ))
}
