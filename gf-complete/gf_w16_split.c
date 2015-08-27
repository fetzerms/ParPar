
#ifdef _MSC_VER
#define ALIGN(_a, v) __declspec(align(_a)) v
#else
#define ALIGN(_a, v) v __attribute__((aligned(_a)))
#endif


/* src can be the same as dest */
static void _FN(gf_w16_split_start)(void* src, int bytes, void* dest) {
	gf_region_data rd;
	_mword *sW, *dW, *topW;
	_mword ta, tb, lmask;
	
	gf_set_region_data(&rd, NULL, src, dest, bytes, 0, 0, sizeof(_mword)*2);
	
	
	if(src != dest) {
		/* copy end and initial parts */
		memcpy(rd.d_top, rd.s_top, (intptr_t)rd.src + rd.bytes - (intptr_t)rd.s_top);
		memcpy(rd.dest, rd.src, (intptr_t)rd.s_start - (intptr_t)rd.src);
	}
	
	sW = (_mword*)rd.s_start;
	dW = (_mword*)rd.d_start;
	topW = (_mword*)rd.d_top;
	
	lmask = _MM(set1_epi16) (0xff);
	
	while(dW != topW) {
		ta = _MMI(load_s)( sW);
		tb = _MMI(load_s)(sW+1);
		
		_MMI(store_s) (dW,
			_MM(packus_epi16)(
				_MM(srli_epi16)(tb, 8),
				_MM(srli_epi16)(ta, 8)
			)
		);
		_MMI(store_s) (dW+1,
			_MM(packus_epi16)(
				_MMI(and_s)(tb, lmask),
				_MMI(and_s)(ta, lmask)
			)
		);
		
		sW += 2;
		dW += 2;
	}
}

/* src can be the same as dest */
static void _FN(gf_w16_split_final)(void* src, int bytes, void* dest) {
	gf_region_data rd;
	_mword *sW, *dW, *topW;
	_mword tpl, tph;
	
	gf_set_region_data(&rd, NULL, src, dest, bytes, 0, 0, sizeof(_mword)*2);
	
	
	if(src != dest) {
		/* copy end and initial parts */
		memcpy(rd.d_top, rd.s_top, (intptr_t)rd.src + rd.bytes - (intptr_t)rd.s_top);
		memcpy(rd.dest, rd.src, (intptr_t)rd.s_start - (intptr_t)rd.src);
	}
	
	sW = (_mword*)rd.s_start;
	dW = (_mword*)rd.d_start;
	topW = (_mword*)rd.d_top;
	
	while(dW != topW) {
		tph = _MMI(load_s)( sW);
		tpl = _MMI(load_s)(sW+1);

		_MMI(store_s) (dW, _MM(unpackhi_epi8)(tpl, tph));
		_MMI(store_s) (dW+1, _MM(unpacklo_epi8)(tpl, tph));
		
		sW += 2;
		dW += 2;
	}
}




typedef union {
	uint8_t u8[MWORD_SIZE];
	uint16_t u16[MWORD_SIZE/2];
	_mword uW;
} gf_mm;

static
void
_FN(gf_w16_split_4_16_lazy_altmap_multiply_region)(gf_t *gf, void *src, void *dest, gf_val_32_t val, int bytes, int xor)
{
#ifdef INTEL_SSSE3
  FAST_U32 i, j, k;
  FAST_U32 prod;
  _mword *sW, *dW, *topW;
  ALIGN(MWORD_SIZE, gf_mm low[4]);
  ALIGN(MWORD_SIZE, gf_mm high[4]);
  gf_region_data rd;
  _mword  mask, ta, tb, ti, tpl, tph;

  if (val == 0) { gf_multby_zero(dest, bytes, xor); return; }
  if (val == 1) { gf_multby_one(src, dest, bytes, xor); return; }

  gf_set_region_data(&rd, gf, src, dest, bytes, val, xor, sizeof(_mword)*2);
  gf_do_initial_region_alignment(&rd);

  for (j = 0; j < 16; j++) {
    for (i = 0; i < 4; i++) {
      prod = gf->multiply.w32(gf, (j << (i*4)), val);
      for (k = 0; k < MWORD_SIZE; k += 16) {
        low[i].u8[j + k] = (uint8_t)prod;
        high[i].u8[j + k] = (uint8_t)(prod >> 8);
      }
    }
  }

  sW = (_mword *) rd.s_start;
  dW = (_mword *) rd.d_start;
  topW = (_mword *) rd.d_top;

  mask = _MM(set1_epi8) (0x0f);

  if (xor) {
    while (dW != topW) {

      ta = _MMI(load_s)(sW);
      tb = _MMI(load_s)(sW+1);

      ti = _MMI(and_s) (mask, tb);
      tph = _MM(shuffle_epi8) (high[0].uW, ti);
      tpl = _MM(shuffle_epi8) (low[0].uW, ti);
  
      ti = _MMI(and_s) (mask, _MM(srli_epi16)(tb, 4));
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[1].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[1].uW, ti), tph);

      ti = _MMI(and_s) (mask, ta);
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[2].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[2].uW, ti), tph);
  
      ti = _MMI(and_s) (mask, _MM(srli_epi16)(ta, 4));
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[3].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[3].uW, ti), tph);

      tph = _MMI(xor_s)(tph, _MMI(load_s)(dW));
      tpl = _MMI(xor_s)(tpl, _MMI(load_s)(dW+1));
      _MMI(store_s) (dW, tph);
      _MMI(store_s) (dW+1, tpl);

      dW += 2;
      sW += 2;
    }
  } else {
    while (dW != topW) {

      ta = _MMI(load_s)(sW);
      tb = _MMI(load_s)(sW+1);

      ti = _MMI(and_s) (mask, tb);
      tph = _MM(shuffle_epi8) (high[0].uW, ti);
      tpl = _MM(shuffle_epi8) (low[0].uW, ti);
  
      ti = _MMI(and_s) (mask, _MM(srli_epi16)(tb, 4));
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[1].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[1].uW, ti), tph);

      ti = _MMI(and_s) (mask, ta);
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[2].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[2].uW, ti), tph);
  
      ti = _MMI(and_s) (mask, _MM(srli_epi16)(ta, 4));
      tpl = _MMI(xor_s)(_MM(shuffle_epi8) (low[3].uW, ti), tpl);
      tph = _MMI(xor_s)(_MM(shuffle_epi8) (high[3].uW, ti), tph);

      _MMI(store_s) (dW, tph);
      _MMI(store_s) (dW+1, tpl);

      dW += 2;
      sW += 2;
      
    }
  }
  gf_do_final_region_alignment(&rd);

#endif
}
