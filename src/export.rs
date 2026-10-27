use arma_file_formats::real_virtuality::paa::Paa;
use gimp::prelude::*;

use crate::error;

enum ExportError {
    /// Invalid input, e.g. dimensions the format cannot store.
    Calling(glib::Error),
    Execution(glib::Error),
}

impl From<glib::Error> for ExportError {
    fn from(e: glib::Error) -> Self {
        Self::Execution(e)
    }
}

/// Run function of the export procedure.
pub fn run(
    procedure: &gimp::ExportProcedure,
    image: &gimp::Image,
    file: &gio::File,
    options: &gimp::ExportOptions,
) -> gimp::ValueArray {
    match export_paa(image, file, options) {
        Ok(()) => procedure.new_return_values(gimp::PDBStatusType::Success, None),
        Err(ExportError::Calling(e)) => {
            procedure.new_return_values(gimp::PDBStatusType::CallingError, Some(e))
        }
        Err(ExportError::Execution(e)) => {
            procedure.new_return_values(gimp::PDBStatusType::ExecutionError, Some(e))
        }
    }
}

fn dimension(value: i32) -> Option<u16> {
    u16::try_from(value).ok().filter(|v| v.is_power_of_two())
}

fn export_paa(
    image: &gimp::Image,
    file: &gio::File,
    options: &gimp::ExportOptions,
) -> Result<(), ExportError> {
    let (Some(width), Some(height)) = (dimension(image.width()), dimension(image.height())) else {
        return Err(ExportError::Calling(error(
            "Error during Paa Export:\nDimensions have to be a power of two (2^n)",
        )));
    };

    // Merges the visible layers and converts to RGB(A), according to the
    // export capabilities.
    let image = options.image(image);
    let layer = image
        .layers()
        .into_iter()
        .next()
        .ok_or_else(|| error("The image has no layers"))?;

    // The layer is in image coordinates and may not cover the whole image,
    // pixels outside of it are transparent.
    let (offset_x, offset_y) = layer.offsets().unwrap_or_default();
    let rect = gimp::gegl::Rectangle::new(-offset_x, -offset_y, width.into(), height.into());
    let format = gimp::babl::Format::format_with_space("R'G'B'A u8", &layer.format().space());
    let buffer = layer.buffer().ok_or_else(|| error("The layer has no buffer"))?;
    let data = buffer.get_pixels(&rect, 1.0, &format, gimp::gegl::AbyssPolicy::None);

    let mut paa = Paa::from_image(width, height, data);
    write_replacing(file, |stream| {
        paa.write(stream, None)
            .map_err(|e| error(format!("Exception during Export: \n {e}")))
    })?;
    Ok(())
}

/// Writes `file` through a replace stream, the existing file is only replaced
/// once `write` succeeded.
fn write_replacing(
    file: &gio::File,
    write: impl FnOnce(&mut gio::OutputStreamWrite<gio::FileOutputStream>) -> Result<(), glib::Error>,
) -> Result<(), glib::Error> {
    let mut stream = file
        .replace(
            None,
            false,
            gio::FileCreateFlags::REPLACE_DESTINATION,
            gio::Cancellable::NONE,
        )?
        .into_write();

    let result = write(&mut stream);
    let stream = stream.into_output_stream();
    match result {
        Ok(()) => stream.close(gio::Cancellable::NONE),
        Err(e) => {
            // Closing a replace stream cancelled keeps the original file.
            let cancellable = gio::Cancellable::new();
            cancellable.cancel();
            let _ = stream.close(Some(&cancellable));
            Err(e)
        }
    }
}
