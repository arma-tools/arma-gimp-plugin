//! GIMP 3 plug-in for the Arma texture formats: loads PAA (Real Virtuality)
//! and EDDS (Enfusion) files and exports PAA files.

// GIMP runs plug-ins in the background, without a console window.
#![windows_subsystem = "windows"]

mod export;
mod load;

use gimp::{prelude::*, subclass::prelude::*};

const PROC_PAA_LOAD: &str = "plug-in-arma-tools-agp-paa-load";
const PROC_PAA_EXPORT: &str = "plug-in-arma-tools-agp-paa-export";
const PROC_EDDS_LOAD: &str = "plug-in-arma-tools-agp-edds-load";

mod imp {
    use super::*;

    #[derive(Default)]
    pub struct ArmaGimpPlugin;

    #[glib::object_subclass]
    impl ObjectSubclass for ArmaGimpPlugin {
        const NAME: &'static str = "ArmaGimpPlugin";
        type Type = super::ArmaGimpPlugin;
        type ParentType = gimp::PlugIn;
    }

    impl ObjectImpl for ArmaGimpPlugin {}

    impl PlugInImpl for ArmaGimpPlugin {
        fn query_procedures(&self) -> Vec<String> {
            vec![
                PROC_PAA_LOAD.into(),
                PROC_PAA_EXPORT.into(),
                PROC_EDDS_LOAD.into(),
            ]
        }

        fn create_procedure(&self, name: &str) -> Option<gimp::Procedure> {
            let plug_in = &*self.obj();
            let procedure: gimp::Procedure = match name {
                PROC_PAA_LOAD => {
                    let procedure = gimp::LoadProcedure::new(
                        plug_in,
                        name,
                        gimp::PDBProcType::Plugin,
                        |procedure, _run_mode, file, _metadata, _flags, _config| {
                            load::run(procedure, file, load::load_paa)
                        },
                    );
                    procedure.set_sensitivity_mask(gimp::ProcedureSensitivityMask::NO_IMAGE);
                    procedure.set_menu_label("PAA image");
                    procedure.set_documentation(
                        "Loads files in the PAA file format",
                        Some("This plug-in loads PAA Texture files."),
                        None,
                    );
                    procedure.set_format_name("PAA");
                    procedure.set_extensions("paa");
                    // Preferred over GIMP's own file-paa loader (GIMP 3.2),
                    // which cannot read DXT1 textures. No magics: GIMP
                    // matches the extension of loaders without magics first,
                    // so they would make file-paa win regardless of priority.
                    procedure.set_priority(-1);
                    procedure.upcast()
                }
                PROC_PAA_EXPORT => {
                    let procedure = gimp::ExportProcedure::new(
                        plug_in,
                        name,
                        gimp::PDBProcType::Plugin,
                        // PAA cannot store Exif/XMP metadata.
                        false,
                        |procedure, _run_mode, image, file, options, _metadata, _config| {
                            export::run(procedure, image, file, options)
                        },
                    );
                    // Other image types are converted, see export::export_paa().
                    procedure.set_image_types("RGB*, GRAY*, INDEXED*");
                    procedure.set_menu_label("PAA image");
                    procedure.set_documentation(
                        "Exports files in the PAA file format",
                        Some("This plug-in exports the image into PAA Texture files."),
                        None,
                    );
                    procedure.set_format_name("PAA");
                    procedure.set_extensions("paa");
                    procedure.set_capabilities(
                        gimp::ExportCapabilities::CAN_HANDLE_RGB
                            | gimp::ExportCapabilities::CAN_HANDLE_ALPHA,
                        None,
                    );
                    procedure.upcast()
                }
                PROC_EDDS_LOAD => {
                    let procedure = gimp::LoadProcedure::new(
                        plug_in,
                        name,
                        gimp::PDBProcType::Plugin,
                        |procedure, _run_mode, file, _metadata, _flags, _config| {
                            load::run(procedure, file, load::load_edds)
                        },
                    );
                    procedure.set_sensitivity_mask(gimp::ProcedureSensitivityMask::NO_IMAGE);
                    procedure.set_menu_label("EDDS image");
                    procedure.set_documentation(
                        "Loads files in the EDDS file format",
                        Some("This plug-in loads EDDS Texture files."),
                        None,
                    );
                    // No magic, "DDS " conflicts with the DDS plug-in.
                    procedure.set_format_name("EDDS");
                    procedure.set_extensions("edds");
                    procedure.set_priority(200);
                    procedure.upcast()
                }
                _ => return None,
            };
            procedure.set_attribution("Willard", "GPL-2.0-or-later", "2025");
            Some(procedure)
        }

        fn set_i18n(&self, _procedure_name: &str) -> Option<I18n> {
            None
        }
    }
}

glib::wrapper! {
    pub struct ArmaGimpPlugin(ObjectSubclass<imp::ArmaGimpPlugin>) @extends gimp::PlugIn;
}

gimp::main!(ArmaGimpPlugin);

fn error(message: impl std::fmt::Display) -> glib::Error {
    glib::Error::new(gimp::PlugInError::Failed, &strip_ansi(&message.to_string()))
}

/// binrw errors are colored with ANSI escape sequences, which GIMP's message
/// dialogs show verbatim.
fn strip_ansi(message: &str) -> String {
    let mut result = String::with_capacity(message.len());
    let mut chars = message.chars();
    while let Some(c) = chars.next() {
        if c == '\x1b' {
            // CSI sequences: ESC '[' parameters final-byte
            if chars.next() == Some('[') {
                for c in chars.by_ref() {
                    if ('@'..='~').contains(&c) {
                        break;
                    }
                }
            }
        } else {
            result.push(c);
        }
    }
    result
}

#[cfg(test)]
mod tests {
    #[test]
    fn strip_ansi() {
        assert_eq!(
            super::strip_ansi("0: \x1b[1m\x1b[1mError: failed\x1b[22m done"),
            "0: Error: failed done"
        );
    }
}
