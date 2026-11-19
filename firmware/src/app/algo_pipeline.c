#include "algo_pipeline.h"
#include <math.h>
#include <string.h>

typedef struct {
    float a[3], r[3], raw_r[3], sigma[3], dt_ms, alpha;
    const AlgoChannelCfg_t *cc;
    const AlgoParams_t *p;
    const AlgoTiming_t *time;
    AlgoState_t *st;
    AlgoOutput_t *out;
    uint16_t enable, bypass;
    bool reference_output;
} Work;
typedef struct { uint8_t id; uint16_t bit; bool calibrated; void (*run)(Work *); } Stage;
static void rotate_one(float v[3],const int8_t r[9]);
static void rotate_noise(float sigma[3],const int8_t r[9])
{
    float variance[3]={0};
    for(unsigned i=0;i<3;i++) for(unsigned j=0;j<3;j++) {
        float k=r[3*i+j]; variance[i]+=k*k*sigma[j]*sigma[j];
    }
    for(unsigned i=0;i<3;i++) sigma[i]=sqrtf(variance[i]);
}

static void load(float v[3], const AlgoVector_t *x) { v[0]=x->x; v[1]=x->y; v[2]=x->z; }
static void store(AlgoVector_t *x, const float v[3]) { x->x=v[0]; x->y=v[1]; x->z=v[2]; }
static bool finite3(const AlgoVector_t *x)
{
    return isfinite(x->x) && isfinite(x->y) && isfinite(x->z) &&
           fabsf(x->x)<=1e10f && fabsf(x->y)<=1e10f && fabsf(x->z)<=1e10f;
}
static uint16_t samples(uint16_t ms, uint16_t rate, uint16_t minimum, uint16_t maximum)
{
    uint32_t n=((uint32_t)ms*rate+500U)/1000U;
    return n<minimum ? minimum : n>maximum ? maximum : (uint16_t)n;
}
void Algo_DefaultParams(AlgoParams_t *p)
{
    memset(p,0,sizeof(*p));
    p->dead_value=8.5f; p->spike_multiplier=20.0f; p->stability_threshold=0.1f;
    p->config=(AlgoConfig_t)ALGO_CONFIG_DEFAULTS;
}
void Algo_Reset(AlgoState_t *s) { memset(s,0,sizeof(*s)); s->factor=1.0f; }

#ifndef ALGO_V2_ONLY
static void legacy_history(Work *w)
{
    AlgoState_t *s=w->st;
    memcpy(s->ref_history[s->ref_index],w->raw_r,sizeof(w->raw_r));
    s->ref_index=(s->ref_index+1)%3;
    if(s->ref_count<3) s->ref_count++;
    if(s->ref_count>=2) for(unsigned j=0;j<3;j++)
        s->velocity[j]=s->ref_history[(s->ref_index+2)%3][j]-s->ref_history[(s->ref_index+1)%3][j];
    if(s->ref_count>=3) for(unsigned j=0;j<3;j++) {
        float prev=s->ref_history[(s->ref_index+1)%3][j]-s->ref_history[s->ref_index][j];
        s->acceleration[j]=s->velocity[j]-prev;
    }
}
#endif
static void zero_bias(Work *w)
{
    float a0[3],r0[3];
    load(a0,&w->cc->calibration.actual_offset); load(r0,&w->cc->calibration.reference_offset);
    if(w->p->config.profile==ALGO_PROFILE_V2 && !(w->bypass&(1U<<1))) {
        rotate_one(a0,w->cc->actual_rotation); rotate_one(r0,w->cc->reference_rotation);
    }
    for(unsigned j=0;j<3;j++) { w->a[j]-=a0[j]; w->r[j]-=r0[j]; }
}
#ifndef ALGO_V2_ONLY
static void legacy_compensate(Work *w)
{
    AlgoState_t *s=w->st;
    if(s->ref_count<2) return;
    float delta=(float)w->cc->legacy_settle_ms/1000.0f,off[3];
    load(off,&w->cc->calibration.reference_offset);
    for(unsigned j=0;j<3;j++) {
        float predicted=s->ref_history[(s->ref_index+2)%3][j]+s->velocity[j]*delta;
        if(s->ref_count>=3) predicted+=0.5f*s->acceleration[j]*delta*delta;
        float d=predicted-off[j];
        if(j==0) w->a[j]-=d; else w->a[j]+=d;
    }
}
static void legacy_dead(Work *w)
{
    float sigma[3]; load(sigma,&w->cc->calibration.actual_std);
    for(unsigned j=0;j<3;j++) if(fabsf(w->a[j])<w->p->dead_value*sigma[j]) w->a[j]=0.0f;
}
static void legacy_z(Work *w) { if(w->a[2]>0.0f) w->a[2]=0.0f; }
static void legacy_average(Work *w)
{
    AlgoState_t *s=w->st;
    uint16_t rate=w->time->timed ? w->time->rate_hz : 10;
    uint16_t n=samples(w->p->config.window_ms,rate,1,ALGO_WINDOW_MAX);
    if(s->window_size!=n) { s->window_index=0; s->window_count=0; s->window_size=n; }
    if(w->reference_output) {
        if(!s->reference_lp_initialized) memset(s->reference_window_valid,0,sizeof(s->reference_window_valid));
        for(unsigned j=0;j<3;j++) s->reference_window[j][s->window_index]=w->r[j];
        s->reference_window_valid[s->window_index]=1;
        s->reference_lp_initialized=1;
    }
    for(unsigned j=0;j<3;j++) s->window[j][s->window_index]=w->a[j];
    s->window_time[s->window_index]=w->time->actual_us;
    s->window_index=(s->window_index+1)%n;
    if(s->window_count<n) s->window_count++;
    /* Re-sum in the old buffer order: running sums change float32 golden bits. */
    for(unsigned j=0;j<3;j++) {
        float sum=0.0f;
        unsigned count=0,reference_count=0;
        float reference_sum=0.0f;
        for(unsigned i=0;i<s->window_count;i++) {
            if(!w->time->timed || (uint32_t)(w->time->actual_us-s->window_time[i])<(uint32_t)w->p->config.window_ms*1000U) {
                sum+=s->window[j][i]; count++;
                if(w->reference_output && s->reference_window_valid[i]) {
                    reference_sum+=s->reference_window[j][i]; reference_count++;
                }
            }
        }
        w->a[j]=sum/count;
        if(reference_count) s->reference_lp[j]=reference_sum/reference_count;
    }
}
static void legacy_mapping(Work *w)
{
    w->a[0]*=w->cc->mapping[0]; w->a[1]*=w->cc->mapping[1];
    w->a[2]=fabsf(w->a[2])*w->cc->mapping[2];
}
static void legacy_stability(Work *w)
{
    AlgoState_t *s=w->st;
    float sigma[3]; load(sigma,&w->cc->calibration.actual_std);
    uint16_t n=samples(w->p->config.stable_ms,w->time->timed?w->time->rate_hz:10,1,65535);
    for(unsigned j=0;j<3;j++) {
        if(fabsf(w->a[j])<3.0f*sigma[j] && fabsf(w->a[j])<w->p->stability_threshold) {
            if(s->legacy_stable[j]<n) s->legacy_stable[j]++;
            s->legacy_stable_ms[j]+=w->dt_ms;
        } else { s->legacy_stable[j]=0; s->legacy_stable_ms[j]=0; s->factor=1.0f; }
    }
    bool stable=w->time->timed ?
        (s->legacy_stable_ms[0]>=w->p->config.stable_ms && s->legacy_stable_ms[1]>=w->p->config.stable_ms && s->legacy_stable_ms[2]>=w->p->config.stable_ms) :
        (s->legacy_stable[0]>=n && s->legacy_stable[1]>=n && s->legacy_stable[2]>=n);
    if(stable) {
        /* Preserve historical 10 Hz bits; time-scale only timed faster streams. */
        float decay=(!w->time->timed || w->time->rate_hz==10) ? 0.9f : expf(-w->dt_ms/w->p->config.tau_ms);
        s->factor*=decay;
        if(s->factor<0.01f) s->factor=0.0f;
        for(unsigned j=0;j<3;j++) w->a[j]*=s->factor;
    }
}
#endif
static void rotate_one(float v[3],const int8_t r[9])
{
    float t[3];
    for(unsigned i=0;i<3;i++) t[i]=r[3*i]*v[0]+r[3*i+1]*v[1]+r[3*i+2]*v[2];
    memcpy(v,t,sizeof(t));
}
static void rotate(Work *w)
{
    rotate_one(w->a,w->cc->actual_rotation); rotate_one(w->r,w->cc->reference_rotation);
}
static float median(const float *values,unsigned n)
{
    float tmp[ALGO_SPIKE_MAD_MAX];
    memcpy(tmp,values,n*sizeof(float));
    for(unsigned i=1;i<n;i++) { float x=tmp[i]; unsigned j=i; while(j && tmp[j-1]>x) { tmp[j]=tmp[j-1]; j--; } tmp[j]=x; }
    return (n&1) ? tmp[n/2] : 0.5f*(tmp[n/2-1]+tmp[n/2]);
}
static void spike_accept(AlgoSpikeState_t *s,float x,float residual,uint16_t m,uint16_t n)
{
    s->med[s->med_index]=x; s->med_index=(s->med_index+1)%m;
    if(s->med_count<m) s->med_count++;
    s->residual[s->mad_index]=residual; s->mad_index=(s->mad_index+1)%n;
    if(s->mad_count<n) s->mad_count++;
}
static float spike_axis(Work *w,AlgoSpikeState_t *s,float x,float floor)
{
    uint16_t rate=w->time->rate_hz ? w->time->rate_hz : 10;
    uint16_t m=samples(w->p->config.spike_med_ms,rate,3,ALGO_SPIKE_MED_MAX);
    uint16_t n=samples(w->p->config.spike_mad_ms,rate,3,ALGO_SPIKE_MAD_MAX);
    uint16_t confirm=samples(w->p->config.spike_confirm_ms,rate,2,65535);
    if(!s->med_count) { spike_accept(s,x,0,m,n); return x; }
    float pred=median(s->med,s->med_count),e=x-pred;
    float noise=s->mad_count ? 1.4826f*median(s->residual,s->mad_count) : floor;
    float th=w->p->config.spike_k*fmaxf(noise,floor);
    if(fabsf(e)<=th) {
        s->pending_count=0; s->pending_ms=0;
        spike_accept(s,x,fabsf(e),m,n); return x;
    }
    s->pending_ms+=w->dt_ms;
    int8_t sign=e>0 ? 1 : -1;
    float low=fminf(s->pending_min,x),high=fmaxf(s->pending_max,x);
    if(!s->pending_count || sign!=s->pending_sign || high-low>th) {
        s->pending_count=1; s->pending_min=x; s->pending_max=x; s->pending_sign=sign;
    } else { s->pending_min=low; s->pending_max=high; s->pending_count++; }
    if(s->pending_count>=confirm || s->pending_ms>=w->p->config.spike_hold_ms) {
        if(s->pending_count>=confirm) w->st->counters.steps++; else w->st->counters.forced_accepts++;
        /* Rebase the predictor, retain the robust noise estimate. */
        for(unsigned i=0;i<m;i++) s->med[i]=x;
        s->med_count=m; s->med_index=0; s->pending_count=0; s->pending_ms=0;
        return x;
    }
    w->st->counters.spikes++; return pred;
}
static void spike(Work *w)
{
    float sa[3],sr[3];
    load(sa,&w->cc->calibration.actual_std); load(sr,&w->cc->calibration.reference_std);
    if(!(w->bypass&(1U<<1))) {
        rotate_noise(sa,w->cc->actual_rotation); rotate_noise(sr,w->cc->reference_rotation);
    }
    for(unsigned j=0;j<3;j++) {
        w->a[j]=spike_axis(w,&w->st->spike[0][j],w->a[j],fmaxf(w->cc->quantum_uT,sa[j]));
        w->r[j]=spike_axis(w,&w->st->spike[1][j],w->r[j],fmaxf(w->cc->quantum_uT,sr[j]));
    }
}
static void compensate(Work *w)
{
    AlgoState_t *s=w->st;
    int32_t skew=w->time->triggered ? (int32_t)(w->time->actual_us-w->time->reference_us) : 0;
    uint32_t ref_dt=w->time->reference_us-s->last_reference_us;
    for(unsigned j=0;j<3;j++) {
        float predicted=w->r[j];
        if(s->ref_initialized && ref_dt>0 && ref_dt<=1000000U)
            predicted+=(w->r[j]-s->previous_ref[j])/(float)ref_dt*(float)skew;
        w->a[j]-=w->p->config.gain[j]*predicted;
    }
}
static void low_pass(Work *w)
{
    AlgoState_t *s=w->st;
    for(unsigned j=0;j<3;j++) {
        if(!s->lp_initialized) s->lp[j]=w->a[j];
        else {
            s->lp[j]+=w->alpha*(w->a[j]-s->lp[j]);
        }
        if(w->reference_output) {
            if(!s->reference_lp_initialized) s->reference_lp[j]=w->r[j];
            else s->reference_lp[j]+=w->alpha*(w->r[j]-s->reference_lp[j]);
        }
        w->a[j]=s->lp[j];
    }
    s->lp_initialized=1;
    if(w->reference_output) s->reference_lp_initialized=1;
}
static void dead_band(Work *w)
{
    for(unsigned j=0;j<3;j++) {
        float t=w->p->dead_value*w->sigma[j],a=fabsf(w->a[j]);
        if(!w->st->db_active[j] && a>t) w->st->db_active[j]=1;
        else if(w->st->db_active[j] && a<0.7f*t) w->st->db_active[j]=0;
        if(!w->st->db_active[j]) w->a[j]=0.0f;
    }
}
static void unipolar(Work *w) { w->a[2]=w->a[2]>0.0f ? 0.0f : -w->a[2]; }
static void mapping(Work *w) { for(unsigned j=0;j<3;j++) w->a[j]*=w->cc->mapping[j]; }
static void stability(Work *w)
{
    AlgoState_t *s=w->st;
    bool stable=true;
    for(unsigned j=0;j<3;j++) {
        float scale=(w->enable&ALGO_MAPPING) && !(w->bypass&(1U<<8)) ? fabsf(w->cc->mapping[j]) : 1.0f;
        if(!(fabsf(w->a[j])<w->p->config.stab_k*w->sigma[j]*scale &&
             fabsf(w->a[j])<w->p->stability_threshold)) stable=false;
    }
    if(!stable) { s->stable_elapsed_ms=0; s->factor=1.0f; return; }
    s->stable_elapsed_ms+=w->dt_ms;
    if(s->stable_elapsed_ms>=w->p->config.stable_ms) {
        s->factor*=expf(-w->dt_ms/w->p->config.tau_ms);
        if(s->factor<0.01f) s->factor=0.0f;
        for(unsigned j=0;j<3;j++) w->a[j]*=s->factor;
    }
}
#ifndef ALGO_V2_ONLY
static const Stage legacy[]={
    {2,ALGO_CALIBRATION,true,zero_bias},{4,ALGO_COMPENSATION,true,legacy_compensate},
    {6,ALGO_DEAD_BAND,true,legacy_dead},{7,ALGO_UNIPOLAR,false,legacy_z},
    {5,ALGO_LOW_PASS,true,legacy_average},{8,ALGO_MAPPING,true,legacy_mapping},
    {9,ALGO_STABILITY,true,legacy_stability}
};
#endif
static const Stage v2[]={
    {1,0,false,rotate},{2,ALGO_CALIBRATION,true,zero_bias},{3,ALGO_SPIKE,false,spike},
    {4,ALGO_COMPENSATION,true,compensate},{5,ALGO_LOW_PASS,false,low_pass},
    {6,ALGO_DEAD_BAND,true,dead_band},{7,ALGO_UNIPOLAR,false,unipolar},
    {8,ALGO_MAPPING,true,mapping},{9,ALGO_STABILITY,true,stability}
};
static void capture(AlgoTrace_t *trace,unsigned id,const Work *w,bool executed)
{
    if(!trace) return;
    memcpy(trace->actual[id],w->a,sizeof(w->a));
    memcpy(trace->reference[id],w->r,sizeof(w->r));
    trace->present|=1U<<id;
    if(executed) trace->executed|=1U<<id;
}
bool Algo_ProcessTrace(const AlgoPair_t *in,const AlgoTiming_t *t,const AlgoChannelCfg_t *cc,
                  const AlgoParams_t *p,uint16_t enable,bool refout,AlgoState_t *s,AlgoOutput_t *out,
                  uint16_t bypass,AlgoTrace_t *trace)
{
#ifdef ALGO_V2_ONLY
    /* Enforce the build policy even for callers bypassing config validation. */
    AlgoParams_t active=*p;
    active.config.profile=ALGO_PROFILE_V2;
    p=&active;
#endif
    if(trace) {
        memset(trace,0,sizeof(*trace)); trace->seq=t->seq; trace->timestamp_us=t->actual_us;
        trace->profile=p->config.profile;
    }
    s->counters.received++; s->counters.stage_runs[0]++;
    out->value.valid=0;
    out->error=t->error;
    out->pair_skew_us=(int32_t)(t->actual_us-t->reference_us);
    if(!in->valid || !finite3(&in->actual) || !finite3(&in->reference) || t->error) {
        s->counters.invalid++;
        if(t->error==1) s->counters.bus_errors++;
        if(t->error==2) s->counters.faults++;
        if(t->error==3) s->counters.timeouts++;
        if(t->error==4) s->counters.skewed++;
        if(!out->error) out->error=5;
        if(trace) trace->error=out->error;
        s->stable_elapsed_ms=0;
#ifndef ALGO_V2_ONLY
        memset(s->legacy_stable_ms,0,sizeof(s->legacy_stable_ms));
#endif
        return false;
    }
    Work w={.cc=cc,.p=p,.time=t,.st=s,.out=out,.enable=enable,.bypass=bypass,.reference_output=refout};
    if(!refout) s->reference_lp_initialized=0;
    load(w.a,&in->actual); load(w.r,&in->reference); memcpy(w.raw_r,w.r,sizeof(w.r));
    capture(trace,0,&w,true);
    uint32_t dt=t->actual_us-s->last_actual_us;
    w.dt_ms=(t->timed && s->clock_initialized && dt>0 && dt<=1000000U) ? dt/1000.0f :
             1000.0f/(t->rate_hz?t->rate_hz:10);
    w.alpha=w.dt_ms/(p->config.tau_lp_ms+w.dt_ms);
    float sa[3],sr[3]; load(sa,&cc->calibration.actual_std); load(sr,&cc->calibration.reference_std);
    if(p->config.profile==ALGO_PROFILE_V2 && !(bypass&(1U<<1))) {
        rotate_noise(sa,cc->actual_rotation); rotate_noise(sr,cc->reference_rotation);
    }
    for(unsigned j=0;j<3;j++) {
        float k=(enable&ALGO_COMPENSATION) && !(bypass&(1U<<4)) && cc->calibration.calibrated ? p->config.gain[j] : 0.0f;
        float variance=sa[j]*sa[j]+k*k*sr[j]*sr[j];
        if((enable&ALGO_LOW_PASS) && !(bypass&(1U<<5))) variance*=w.alpha/(2.0f-w.alpha);
        w.sigma[j]=sqrtf(variance); out->sigma_eff[j]=w.sigma[j];
    }
#ifdef ALGO_V2_ONLY
    const Stage *stages=v2;
    unsigned n=sizeof(v2)/sizeof(v2[0]);
#else
    if(p->config.profile==ALGO_PROFILE_LEGACY && cc->calibration.calibrated) legacy_history(&w);
    const Stage *stages=p->config.profile==ALGO_PROFILE_V2 ? v2 : legacy;
    unsigned n=p->config.profile==ALGO_PROFILE_V2 ? sizeof(v2)/sizeof(v2[0]) : sizeof(legacy)/sizeof(legacy[0]);
#endif
    out->reference_zero=in->reference;
    for(unsigned i=0;i<n;i++) {
        const Stage *stage=&stages[i];
        bool executed=!(bypass&(1U<<stage->id)) &&
            (!stage->bit || (enable&stage->bit)) && (!stage->calibrated || cc->calibration.calibrated);
        if(executed) {
            stage->run(&w); s->counters.stage_runs[stage->id]++;
        }
        capture(trace,stage->id,&w,executed);
        if(stage->id==2) store(&out->reference_zero,w.r);
        if(stage->id==4) {
            memcpy(s->previous_ref,w.r,sizeof(w.r)); s->last_reference_us=t->reference_us; s->ref_initialized=1;
        }
    }
    for(unsigned j=0;j<3;j++) if(!isfinite(w.a[j]) || !isfinite(w.r[j])) {
        s->counters.invalid++; out->error=5; if(trace) trace->error=5; return false;
    }
    out->value=*in; store(&out->value.actual,w.a);
    store(&out->value.reference,refout && (enable&ALGO_LOW_PASS) && !(bypass&(1U<<5)) &&
          (p->config.profile==ALGO_PROFILE_V2 || cc->calibration.calibrated) ? s->reference_lp : w.r);
    out->value.valid=1;
    if(trace) trace->valid=1;
    s->last_actual_us=t->actual_us; s->clock_initialized=1;
    s->counters.accepted++;
    return true;
}
bool Algo_Process(const AlgoPair_t *in,const AlgoTiming_t *t,const AlgoChannelCfg_t *cc,
                  const AlgoParams_t *p,uint16_t enable,bool refout,AlgoState_t *s,AlgoOutput_t *out)
{
    return Algo_ProcessTrace(in,t,cc,p,enable,refout,s,out,0,NULL);
}
