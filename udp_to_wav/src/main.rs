extern crate hound;

use std::net::UdpSocket;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::time::{Duration, Instant};




fn main() {

    let spec = hound::WavSpec {
        channels: 1,
        sample_rate: 8000,
        bits_per_sample: 8,
        sample_format: hound::SampleFormat::Int,
    };

    let mut writer = hound::WavWriter::create("serialSound.wav", spec).unwrap();

    let socket = UdpSocket::bind("10.0.0.111:51803").unwrap();

    let mut byte_count: u64 = 0;

    let mut sound_data: Vec<u8> = Vec::new();

    let mut old_origin = SocketAddr::new(IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1)), 8080);
    let mut last_host_change : Instant = Instant::now();

    while byte_count < 200000 {
        let mut buffer = [0; 1000];

        let (num_bytes, origin) = socket.recv_from(&mut buffer).unwrap();

        if origin != old_origin && last_host_change.elapsed() > Duration::from_millis(100) {
            old_origin = origin;
            last_host_change = Instant::now();
            println!("Locked to new master: {}", origin);
        }

        if origin == old_origin {
            for i in 0..num_bytes {
                sound_data.push(buffer[i]);
            }

            byte_count = byte_count + num_bytes as u64;
            println!("Recieved {} bytes so far from {}...", byte_count, origin);
        }

    }

    for byte in sound_data {
        //println!("{}", ((byte as i16)-127) as i8 );
        writer.write_sample( ((byte as i16)-127) as i8).unwrap();
    }

    writer.finalize().unwrap();
}
