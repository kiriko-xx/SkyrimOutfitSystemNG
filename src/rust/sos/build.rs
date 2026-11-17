use std::{
    fs::File,
    io::{BufReader, BufWriter, Write},
};

extern crate core;

fn main() {
    let includes = std::env::var("INCLUDE_PATHS").expect("No value for INCLUDE_PATHS");
    let defines = std::env::var("DEFINITIONS").expect("No value for DEFINITIONS");
    let options = std::env::var("OPTIONS").expect("No value for OPTIONS");

    let mut cxx = cxx_build::bridge("src/interface.rs");

    cxx.no_default_flags(true);

    cxx.include(includes).flag("/std:c++20").flag("/EHsc");

    for option in options.split(' ') {
        cxx.flag_if_supported(option);
    }

    for define in defines.split(' ') {
        let pair = define.split_once('=').expect("Failed to split at =");
        cxx.define(pair.0, Some(pair.1));
    }
    cxx.define("RUST_DEFINES", None);

    // let profile = std::env::var("PROFILE").unwrap();

    cxx.compile("sos");

    let header_out = std::env::var("OUT_DIR").expect("No value for OUT_DIR");
    let header_dest = std::env::var("HEADER_GEN").expect("No value for HEADER_GEN");

    let expected_header = header_out + "/cxxbridge/include/sos/src/interface.rs.h";
    println!("Looking for header in {}", expected_header);
    std::fs::create_dir(header_dest.clone()).ok();

    // Open the generated header and write it to the destination, adding some warning disables to the header.
    let mut input_file =
        BufReader::new(File::open(expected_header).expect("Could not open generated header"));
    let mut output_file = BufWriter::new(
        File::create(header_dest + "/bindings.h.tmp").expect("Could not create destination header"),
    );
    writeln!(output_file, "#pragma warning( push )").expect("Could not write to output header");
    writeln!(output_file, "#pragma warning( disable : 4114 )")
        .expect("Could not write to output header");
    std::io::copy(&mut input_file, &mut output_file).expect("Could not write to output header.");
    writeln!(output_file, "#pragma warning( pop )").expect("Could not write to output header");
    // std::fs::copy(expected_header, header_dest + "/bindings.h.tmp").unwrap();
}
