# ppmdescreen

Descreen filter for PPM images.

## Compile

### Depends

Edit FFTW and possibly FFTWINC and FFTWLIB in Makefile to refect where you installed fftw (www.fftw.org).

### Build

Type `make`

You're done

## Usage

The power spectrum.

```shell
ppmdescreen -i sample.ppm -p ps.ppm
```

Now you can either paint over the spots in the power spectrum yourself, or ask undither to do it for you.

```shell
ppmdescreen -i sample.ppm -a 0.01 -p akill.ppm
ppmdescreen -i sample.ppm -k akill.ppm -o automatic.ppm
```
or better still do it all in one step
```shell
ppmdescreen -i sample.ppm -a 0.01 -o automatic.ppm
```

This does a pretty good job, but it can't get the monochrome spots.

With a little help from the [gimp](https://www.gimp.org/) I've drawn over the four white spots in akill.ppm and called it mkill.ppm.
```shell
ppmdescreen -i sample.ppm -k mkill.ppm -o manual.ppm
```

## Links

Homepage: http://www.madingley.org/james/resources/undither/
GIT: https://github.com/ImageProcessing-ElectronicPublications/ppmdescreen
