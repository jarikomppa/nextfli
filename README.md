# Special FLIC-like encoder for Spectrum Next

by Jari Komppa, http://iki.fi/sol

This is an animation encoder / compressor designed to generate files for the .playflx dot command on the ZX Spectrum Next.

As such it's fixed to the ZX Spectrum Next limitations, including display resolution and palette bit depth.

The encoder can output FLC and GIF files as well for comparison, but the same fixed limitations exist.

The flx format is still a bit in flux, so try not to rely on the results quite yet.

## Understanding FLX

The flx format is first and foremost a delta encoder: the less subsequent frames change, the better the compression ratio. 
Additionally it includes RLE encoding, meaning that it likes large areas of a single color.

Compared to FLI/FLC, the FLX can also copy from different offsets of the previous frame, meaning that scrolling backgrounds
compress fairly well. FLX uses several different encoding schemes which have been found to work better with different workloads,
swapping between encoders on frame by frame basis. FLX files take typically about 75% of their FLC counterparts.

You may find limited success with full motion video (FMV). The best results there can be achieved when camera is standing
still and only part of the image is in motion, like someone sitting and talking. When in doubt, try using the lossy 
compression option and/or reduce video size on screen.

If compression is low, you may find that the zx spectrum next cannot decompress the frames fast enough for fluid video.
For some workloads this is unavoidable. You can try using lossy compression and/or reducing video size in these cases.

An occasional slow frame does not matter as playflx pre-decodes as many frames as it can fit in memory.

## Options

### -h --help

Print help and exit

### -f --flc

Output a FLC file. This is a standard autodesk FLC file which can be played with whatever plays FLC files.

This output mode exists primarily for comparison, and is not actively stressed, so changes in the encoder
may break it at some point without nobody noticing.

The same limitations (palette depth, resolution) affect the FLC output.

### -x --flx

Output nextfli FLX format. This is the default.

### -g --gif

Output GIF format. The gif encoder is not optimal, and the resulting files are overly large. Use
gifsicle or some other gif optimizer if you're interested in the file size.

This output mode exists primarily for easy preview of the image quality on a PC, or for posting
videos online.

The same limitations (palette depth, resolution) affect the GIF output.

### -s --std

Use standard blocks. Using this on FLX output will produce broken flx files. It exists solely
to see how the flx encoders fare compared to the standard flc ones. Sometimes the standard
delta8 frame produces smaller frames than any of the FLX ones.

### -e --ext

Use extended blocks. Using this on FLC output will produce bloken flc files. This option
exists for the same reason as the previous one.

### -q --quick
 
Compress faster, producing bigger files. This option only uses a couple of FLX compression
options. Useful for iterating, but final files should be encoded without it.

### -l --halfres

Reduce output resolution to half x, half y, with huge black border around it.

### -d --dither

Use ordered dither. May (or may not) produce nicer looking output, but likely makes the output
bigger.

### -p --fastscale

Use fast image scaling. In case the input files are not the correct size, nextfli will scale the
images to the target resolution. The default high-quality scaling may be slow, especially on 
large number of input frames. This option will make it faster, but result will also be uglier.

It's a good idea to pre-scale the input frames if possible.

### -v --verify

Verify that encoding is correct. Use if you suspect a bug in nextfli.
 
### -t --threads

Set the number of threads to use. By default, thread count is equal to logical cores in the system.
If you're planning to use the computer while encoding, you may wish to set this lower.

### -r --framedelay

How many frames to delay between displayed frames. 1=50hz, 2=25hz, 3=16.7hz, 4=12.5hz, 5=10hz, etc. Defaults to 4.

### -i --info

Output a log file showing some information about the encoded frames. Most likely completely useless to you.

### -m --minspan

Set minimum span length. May be used to trade file size for faster decode. Use with caution, results are likely
worse than better.

The logic behind this option is that since flx attempts to achieve as high compression ratio as possible, it may
cut a run in pieces just to save one byte of space. The more runs there are, the slower the decode. On the other
hand, the larger the file, the slower the decode, too.

If you wish to try this option, values around 10 or less may be useful.

### -L --lossy

Apply a lossy filter over frames to compress better. This helps with live-action or full motion video.

The filter leaves pixels unchanged if they change less than specified amount. The amount is the manhattan 
distance between pixels, so a value of 1 means that either red, green or blue may differ by 1; a value of 
3 would mean that any combination of changes in any of the channels, as long as the sum is 3 or less, is 
ignored.

Value 2 seems to be fine. Your mileage may vary.

### -K --keyframes

Insert keyframes where the lossy filter is not applied to the file. 

If you experience a lot of smearing or other image corruption when using lossy option above, this can be
used to mitigate the effect. The result will naturally be larger files.

A value of 20 would add a keyframe every 20 frames. By default no keyframes are applied.

### -c --colors

Number of colors in the resulting palette. Defaults to 256, which is also the maximum. Using reduced
number of colors may lead to larger continious areas and thus improve compression, while making the
image quality worse. When reducing palette a lot, using dithering may help with image quality but will
also make compression worse.

## Linux and Mac support

The source currently builds on windows. Everything is relatively portable except for thread-related stuff.

Pull requests are welcome.

## Why is it so slow?

Nextfli uses a brute force algorithm to find the best spans to copy from the previous frame. This is done
separately for every compression encoding. Using fast mode reduces the number of compression encodings
to use.

## How do I convert from video file to nextfli?

First, separate all the frames to individual image files, preferably in the target resolution (e.g, 256x192).
This can be done using ffmpeg, for example:

	ffmpeg -i foo.mp4 -vf fps=50/4,scale=256:192 foo%%04d.png

That command tells ffmpeg to output frames at the desired rate (here, 1/4 of 50hz), scale the frames to the
desired resolution (256x192) and output the separate frames as png files.

Then simply feed those frames to nextfli, possibly tweaking options until you're either satisfied or
disgusted.

## I love it, where's the donate button?

The Finnish law unfortunately forbids me from asking for donations.

I know, it's stupid. Blame the national church, who accept donations and love the status quo. Maybe they'll change the laws one day.

That said, nothing stops me from accepting gifts, but I'm not suggesting you do anything.
