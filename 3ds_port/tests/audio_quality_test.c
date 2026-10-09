#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "3ds_audio_cadence.h"
#include "3ds_cgb_audio.h"
enum {
    SOUND_MODE_DA_BIT_8=0x00900000, SOUND_MODE_FREQ_13379=0x00040000,
    SOUND_MODE_FREQ_31536=0x00090000, SOUND_MODE_FREQ=0x000F0000,
    SOUND_MODE_MASVOL_SHIFT=12, SOUND_MODE_MAXCHN_SHIFT=8,
    PLANE=1584, N=8192
};
static uint32_t selectedMode;
static void m4aSoundMode(uint32_t mode) { selectedMode=mode; }
static void SelectMode(void)
{
/* PRODUCTION MODE CALL */
}
static void CheckRing(unsigned samples,unsigned rate)
{
    unsigned period=PLANE/samples;
    uint8_t counter=0;
    unsigned visits[8]={0};
    assert(period>0 && period<=8 && samples<=1024);
    for (unsigned frame=0;frame<30000;++frame) {
        counter=CtrAudio_VSyncNext(counter,(uint8_t)period);
        unsigned offset=CtrAudio_PcmSliceOffset(counter,period,samples);
        assert(offset%samples==0 && offset+samples<=PLANE);
        visits[offset/samples]++;
    }
    for (unsigned slot=0;slot<period;++slot)
        assert(visits[slot]>=30000/period && visits[slot]<=30000/period+1);
    /* m4a's real rate formula: frames still run at about 59.7275 Hz. */
    assert((597275u*samples+5000)/10000==rate);
    assert(fabs((double)rate/samples-59.7275)<0.003);
}
static double Power(const int16_t *samples,unsigned rate,double hz)
{
    const double pi=acos(-1.0);
    double re=0,im=0;
    for (unsigned i=0;i<N;++i) {
        double window=0.5-0.5*cos(2*pi*i/(N-1));
        double phase=2*pi*hz*i/rate;
        re+=samples[i]*window*cos(phase);
        im+=samples[i]*window*sin(phase);
    }
    return re*re+im*im;
}
static double CaptureHarmonic(unsigned rate,unsigned slice)
{
    uint8_t regs[0x400]={0};
    static CtrCgb cgb;
    static int16_t pcm[2*1024],capture[N];
    CtrCgb_Init(&cgb,rate); CtrCgb_SetBandLimited(&cgb,1);
    regs[CTR_CGB_REG_NR52]=0x80; regs[CTR_CGB_REG_NR50]=0x77;
    regs[CTR_CGB_REG_NR51]=0x11; regs[CTR_CGB_REG_SOUNDCNT_H]=0x0E;
    regs[CTR_CGB_REG_NR11]=2<<6; regs[CTR_CGB_REG_NR12]=0xF0;
    regs[CTR_CGB_REG_NR13]=2016&255; regs[CTR_CGB_REG_NR14]=0x80|(2016>>8);
    /* Real PSG synthesis, 4096-Hz pulse. Its third harmonic is 12288 Hz:
     * outside the old 6689-Hz Nyquist limit, inside the new 15768-Hz limit. */
    for (unsigned done=0;done<N;) {
        assert(CtrCgb_Render(&cgb,regs,pcm,slice)==slice);
        for (unsigned i=0;i<slice && done<N;++i) {
            assert(pcm[2*i]==pcm[2*i+1]);
            assert(pcm[2*i]>-12000 && pcm[2*i]<12000);
            capture[done++]=pcm[2*i];
        }
    }
    double db=10*log10(Power(capture,rate,12288)/Power(capture,rate,4096));
    printf("PSG rate=%u: third/fundamental %.2f dB\n",rate,db);
    return db;
}
int main(void)
{
    SelectMode();
    assert((selectedMode&0xF000)==12u<<SOUND_MODE_MASVOL_SHIFT);
    assert((selectedMode&0xF00)==5u<<SOUND_MODE_MAXCHN_SHIFT);
#ifdef PLATFORM_3DS
    assert((selectedMode&SOUND_MODE_FREQ)==SOUND_MODE_FREQ_31536);
#else
    assert((selectedMode&SOUND_MODE_FREQ)==SOUND_MODE_FREQ_13379);
#endif
    CheckRing(224,13379); CheckRing(528,31536);
    assert(3*528==PLANE);
    double previous=CaptureHarmonic(13379,224),current=CaptureHarmonic(31536,528);
    assert(current>-18 && current<-5);
    assert(current>previous+15);
    puts("PASS audio quality: native/GBA selection, unchanged gain/channels, 30000-frame rings and restored treble harmonic");
}
