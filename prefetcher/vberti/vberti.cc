#include "vberti.h"

#define LANZAR_INT 8

void vberti::notify_prefetch(uint64_t addr, uint64_t cycle) { 
    latency_table_add(addr, 0, 0, cycle & TIME_MASK); 
}

bool vberti::compare_greater_stride_t(stride_t a, stride_t b)
{
  if (a.rpl == L1 && b.rpl != L1)
    return 1;
  else if (a.rpl != L1 && b.rpl == L1)
    return 0;
  else {
    if (a.rpl == L2 && b.rpl != L2)
      return 1;
    else if (a.rpl != L2 && b.rpl == L2)
      return 0;
    else {
      if (a.rpl == L2R && b.rpl != L2R)
        return 1;
      if (a.rpl != L2R && b.rpl == L2R)
        return 0;
      else {
        if (std::abs(a.stride) < std::abs(b.stride))
          return 1;
        return 0;
      }
    }
  }
}

bool vberti::compare_greater_stride_t_per(stride_t a, stride_t b)
{
  if (a.per > b.per)
    return 1;
  else {
    if (std::abs(a.stride) < std::abs(b.stride))
      return 1;
    return 0;
  }
}

/******************************************************************************/
/*                      Latency table functions                               */
/******************************************************************************/
void vberti::latency_table_init()
{
  for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
    latencyt[i].tag = 0;
    latencyt[i].addr = 0;
    latencyt[i].time = 0;
    latencyt[i].pf = 0;
  }
}

uint64_t vberti::latency_table_get_ip(uint64_t line_addr)
{
  for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
    if (latencyt[i].addr == line_addr && latencyt[i].tag)
      return latencyt[i].tag;
  }
  return 0;
}

uint8_t vberti::latency_table_add(uint64_t line_addr, uint64_t tag, uint8_t pf)
{
  return latency_table_add(line_addr, tag, pf, current_cycle & TIME_MASK);
}

uint8_t vberti::latency_table_add(uint64_t line_addr, uint64_t tag, uint8_t pf, uint64_t cycle)
{
  latency_table_t* free = nullptr;

  for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
    if (latencyt[i].addr == line_addr) {
      latencyt[i].time = cycle;
      latencyt[i].tag = tag;
      latencyt[i].pf = pf;
      return latencyt[i].pf;
    }

    if (latencyt[i].tag == 0)
      free = &latencyt[i];
  }

  if (free == nullptr)
    return 0;

  free->addr = line_addr;
  free->time = cycle;
  free->tag = tag;
  free->pf = pf;

  return free->pf;
}

uint64_t vberti::latency_table_del(uint64_t line_addr)
{
  for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
    if (latencyt[i].addr == line_addr) {
      uint64_t latency = (current_cycle & TIME_MASK) - latencyt[i].time;

      latencyt[i].tag = 0;
      latencyt[i].time = 0;
      latencyt[i].pf = 0;

      return latency;
    }
  }
  return 0;
}

uint64_t vberti::latency_table_get(uint64_t line_addr)
{
  for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
    if (latencyt[i].addr == line_addr)
      return latencyt[i].time;
  }
  return 0;
}

/******************************************************************************/
/*                       Shadow Cache functions                               */
/******************************************************************************/
void vberti::shadow_cache_init()
{
  for (uint32_t i = 0; i < L1D_SET; i++) {
    for (uint32_t ii = 0; ii < L1D_WAY; ii++) {
      scache[i][ii].addr = 0;
      scache[i][ii].lat = 0;
      scache[i][ii].pf = 0;
    }
  }
}

uint8_t vberti::shadow_cache_add(uint32_t set, uint32_t way, uint64_t line_addr, uint8_t pf, uint64_t latency)
{
  scache[set][way].addr = line_addr;
  scache[set][way].pf = pf;
  scache[set][way].lat = latency;
  return scache[set][way].pf;
}

uint8_t vberti::shadow_cache_get(uint64_t line_addr)
{
  for (uint32_t i = 0; i < L1D_SET; i++) {
    for (uint32_t ii = 0; ii < L1D_WAY; ii++) {
      if (scache[i][ii].addr == line_addr)
        return 1;
    }
  }
  return 0;
}

uint8_t vberti::shadow_cache_pf(uint64_t line_addr)
{
  for (uint32_t i = 0; i < L1D_SET; i++) {
    for (uint32_t ii = 0; ii < L1D_WAY; ii++) {
      if (scache[i][ii].addr == line_addr) {
        scache[i][ii].pf = 0;
        return 1;
      }
    }
  }
  return 0;
}

uint8_t vberti::shadow_cache_is_pf(uint64_t line_addr)
{
  for (uint32_t i = 0; i < L1D_SET; i++) {
    for (uint32_t ii = 0; ii < L1D_WAY; ii++) {
      if (scache[i][ii].addr == line_addr)
        return scache[i][ii].pf;
    }
  }
  return 0;
}

uint8_t vberti::shadow_cache_latency(uint64_t line_addr)
{
  for (uint32_t i = 0; i < L1D_SET; i++) {
    for (uint32_t ii = 0; ii < L1D_WAY; ii++) {
      if (scache[i][ii].addr == line_addr)
        return scache[i][ii].lat;
    }
  }
  assert(0);
  return 0;
}

/******************************************************************************/
/*                       History Table functions                               */
/******************************************************************************/
void vberti::history_table_init()
{
  for (uint32_t i = 0; i < HISTORY_TABLE_SET; i++) {
    history_pointers[i] = historyt[i];

    for (uint32_t ii = 0; ii < HISTORY_TABLE_WAY; ii++) {
      historyt[i][ii].tag = 0;
      historyt[i][ii].time = 0;
      historyt[i][ii].addr = 0;
    }
  }
}

void vberti::history_table_add(uint64_t tag, uint64_t addr)
{
  uint16_t set = tag & TABLE_SET_MASK;
  addr &= ADDR_MASK;

  uint64_t cycle = current_cycle & TIME_MASK;
  
  history_pointers[set]->tag = tag;
  history_pointers[set]->time = cycle;
  history_pointers[set]->addr = addr;

  if (history_pointers[set] == &historyt[set][HISTORY_TABLE_WAY - 1]) {
    history_pointers[set] = &historyt[set][0];
  } else
    history_pointers[set]++;
}

uint16_t vberti::history_table_get_aux(uint32_t latency, uint64_t tag, uint64_t act_addr, 
                                       uint64_t ip[HISTORY_TABLE_WAY], uint64_t addr[HISTORY_TABLE_WAY], uint64_t cycle)
{
  uint16_t num_on_time = 0;
  uint16_t set = tag & TABLE_SET_MASK;

  if (cycle < latency)
    return num_on_time;
  cycle -= latency;

  history_table_t* pointer = history_pointers[set];

  do {
    if (pointer->tag == tag && pointer->time <= cycle) {
      if (pointer->addr == act_addr)
        return num_on_time;

      int found = 0;
      for (int i = 0; i < num_on_time; i++) {
        if (pointer->addr == addr[i])
          return num_on_time;
      }

      ip[num_on_time] = pointer->tag;
      addr[num_on_time] = pointer->addr;
      num_on_time++;
    }

    if (pointer == historyt[set]) {
      pointer = &historyt[set][HISTORY_TABLE_WAY - 1];
    } else
      pointer--;
  } while (pointer != history_pointers[set]);

  return num_on_time;
}

uint16_t vberti::history_table_get(uint32_t latency, uint64_t tag, uint64_t act_addr, 
                                  uint64_t ip[HISTORY_TABLE_WAY], uint64_t addr[HISTORY_TABLE_WAY], uint64_t cycle)
{
  act_addr &= ADDR_MASK;
  uint16_t num_on_time = history_table_get_aux(latency, tag, act_addr, ip, addr, cycle);
  return num_on_time;
}

/******************************************************************************/
/*                      VBerti table functions                               */
/******************************************************************************/
void vberti::vberti_increase_conf_ip(uint64_t tag)
{
  if (vbertit.find(tag) == vbertit.end())
    return;

  vberti_t* tmp = vbertit[tag];
  stride_t* aux = tmp->stride;

  tmp->conf += CONFIDENCE_INC;

  if (tmp->conf == CONFIDENCE_MAX) {
    for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
      float temp = (float)aux[i].conf / (float)tmp->conf;
      uint64_t aux_conf = (uint64_t)(temp * 100);

      if (aux_conf > CONFIDENCE_L1)
        aux[i].rpl = L1;
      else if (aux_conf > CONFIDENCE_L2)
        aux[i].rpl = L2;
      else if (aux_conf > CONFIDENCE_L2R)
        aux[i].rpl = L2R;
      else
        aux[i].rpl = R;

      aux[i].conf = 0;
    }
    tmp->conf = 0;
  }
}

void vberti::vberti_table_add(uint64_t tag, int64_t stride)
{
  if (vbertit.find(tag) == vbertit.end()) {
    if (vbertit_queue.size() > BERTI_TABLE_SIZE) {
      uint64_t key = vbertit_queue.front();
      vberti_t* tmp = vbertit[key];
      delete tmp->stride;
      delete tmp;
      vbertit.erase(vbertit_queue.front());
      vbertit_queue.pop();
    }
    vbertit_queue.push(tag);

    assert(vbertit.size() <= BERTI_TABLE_SIZE);

    vberti_t* tmp = new vberti_t;
    tmp->stride = new stride_t[BERTI_TABLE_STRIDE_SIZE]();

    tmp->conf = CONFIDENCE_INC;

    tmp->stride[0].stride = stride;
    tmp->stride[0].conf = CONFIDENCE_INIT;
    tmp->stride[0].rpl = R;

    vbertit.insert(make_pair(tag, tmp));
    return;
  }

  vberti_t* tmp = vbertit[tag];
  stride_t* aux = tmp->stride;

  uint8_t max = 0;

  for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
    if (aux[i].stride == stride) {
      aux[i].conf += CONFIDENCE_INC;
      if (aux[i].conf > CONFIDENCE_MAX)
        aux[i].conf = CONFIDENCE_MAX;
      return;
    }
  }

  uint8_t dx_conf = 100;
  int dx_remove = -1;
  for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
    if (aux[i].rpl == R && aux[i].conf < dx_conf) {
      dx_conf = aux[i].conf;
      dx_remove = i;
    }
  }

  if (dx_remove > -1) {
    tmp->stride[dx_remove].stride = stride;
    tmp->stride[dx_remove].conf = CONFIDENCE_INIT;
    tmp->stride[dx_remove].rpl = R;
    return;
  } else {
    for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
      if (aux[i].rpl == L2R && aux[i].conf < dx_conf) {
        dx_conf = aux[i].conf;
        dx_remove = i;
      }
    }
    if (dx_remove > -1) {
      tmp->stride[dx_remove].stride = stride;
      tmp->stride[dx_remove].conf = CONFIDENCE_INIT;
      tmp->stride[dx_remove].rpl = R;
      return;
    }
  }
}

uint8_t vberti::vberti_table_get(uint64_t tag, stride_t res[MAX_PF])
{
  if (!vbertit.count(tag))
    return 0;

  vberti_t* tmp = vbertit[tag];
  stride_t* aux = tmp->stride;
  uint64_t max_conf = 0;
  uint16_t dx = 0;

  for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
    if (aux[i].stride != 0 && aux[i].rpl) {
      res[dx].stride = aux[i].stride;
      res[dx].rpl = aux[i].rpl;
      dx++;
    }
  }

  if (dx == 0 && tmp->conf >= LANZAR_INT) {
    for (int i = 0; i < BERTI_TABLE_STRIDE_SIZE; i++) {
      if (aux[i].stride != 0) {
        res[dx].stride = aux[i].stride;
        float temp = (float)aux[i].conf / (float)tmp->conf;
        uint64_t aux_conf = (uint64_t)(temp * 100);
        res[dx].per = aux_conf;
        dx++;
      }
    }
    sort(res, res + MAX_PF, [this](stride_t a, stride_t b){ return compare_greater_stride_t_per(a, b); });

    for (int i = 0; i < MAX_PF; i++) {
      if (res[i].per > 80)
        res[i].rpl = L1;
      else if (res[i].per > 35)
        res[i].rpl = L2;
      else
        res[i].rpl = R;
    }
    sort(res, res + MAX_PF, [this](stride_t a, stride_t b){ return compare_greater_stride_t(a, b); });
    return 1;
  }

  sort(res, res + MAX_PF, [this](stride_t a, stride_t b){ return compare_greater_stride_t(a, b); });
  return 1;
}

void vberti::find_and_update(uint64_t latency, uint64_t tag, uint64_t cycle, uint64_t line_addr)
{
  uint64_t ip[HISTORY_TABLE_WAY];
  uint64_t addr[HISTORY_TABLE_WAY];
  uint16_t num_on_time = 0;

  num_on_time = history_table_get(latency, tag, line_addr, ip, addr, cycle);

  for (uint32_t i = 0; i < num_on_time; i++) {
    if (i == 0)
      vberti_increase_conf_ip(tag);

    if (i >= MAX_HISTORY_IP)
      break;

    int64_t stride;
    line_addr &= ADDR_MASK;

    stride = (int64_t)(line_addr - addr[i]);

    if ((std::abs(stride) < (1 << STRIDE_MASK))) {
      vberti_table_add(ip[i], stride);
    }
  }
}

void vberti::prefetcher_initialize()
{
  std::cout << "Cache Size: " << L1D_SET << "*" << L1D_WAY << std::endl;
  shadow_cache_init();
  latency_table_init();
  history_table_init();
  
  std::cout << "History Sets: " << HISTORY_TABLE_SET << std::endl;
  std::cout << "History Ways: " << HISTORY_TABLE_WAY << std::endl;
  std::cout << "BERTI Size: " << BERTI_TABLE_SIZE << std::endl;
  std::cout << "BERTI Stride Size: " << BERTI_TABLE_STRIDE_SIZE << std::endl;
}

uint32_t vberti::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, 
                                         bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  uint64_t ip_val = ((ip.to<uint64_t>() >> 1) ^ (ip.to<uint64_t>() >> 4)) & IP_MASK;

  if (!cache_hit) {
    // This is a miss
    latency_table_add(line_addr, ip_val, 1);
    history_table_add(ip_val, line_addr);
  } else if (cache_hit && shadow_cache_is_pf(line_addr)) {
    // Cache line access
    shadow_cache_pf(line_addr);

    // Update stride patterns
    uint64_t latency = shadow_cache_latency(line_addr);
    find_and_update(latency, ip_val, current_cycle & TIME_MASK, line_addr);

    history_table_add(ip_val, line_addr);
  } else {
    // Cache line access
    shadow_cache_pf(line_addr);
  }

  // Get stride to prefetch
  stride_t stride[MAX_PF];
  for (int i = 0; i < MAX_PF; i++) {
    stride[i].conf = 0;
    stride[i].stride = 0;
    stride[i].rpl = R;
  }

  if (!vberti_table_get(ip_val, stride))
    return metadata_in;

  int launched = 0;
  for (int i = 0; i < MAX_PF_LAUNCH; i++) {
    uint64_t p_addr = (line_addr + stride[i].stride) << LOG2_BLOCK_SIZE;
    uint64_t p_b_addr = (p_addr >> LOG2_BLOCK_SIZE);

    if (!latency_table_get(p_addr)) {
      // Determine prefetch level based on confidence
      if (stride[i].rpl == L1 || stride[i].rpl == L2 || stride[i].rpl == L2R) {
        champsim::address prefetch_addr{p_addr};
        
        // Issue prefetch
        if (prefetch_line(prefetch_addr, true, 0)) {
          if (abs(stride[i].stride) > 63) 
            calcu_high++;
          else 
            calcu_low++;
          launched++;
        }
      }
    }
  }
  
  return metadata_in;
}

uint32_t vberti::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, 
                                      champsim::address evicted_addr, uint32_t metadata_in)
{
  uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);

  // Remove from latency table
  uint64_t tag = latency_table_get_ip(line_addr);
  uint64_t cycle = latency_table_get(line_addr);
  uint64_t latency = latency_table_del(line_addr);

  if (latency > LAT_MASK)
    latency = 0;

  // Add to the shadow cache
  shadow_cache_add(set, way, line_addr, prefetch, latency);

  if (latency != 0 && !prefetch) {
    find_and_update(latency, tag, cycle, line_addr);
  }

  return metadata_in;
}

void vberti::prefetcher_final_stats() { 
  cout << "Offset larger than 7 bits: " << calcu_high << endl;
  cout << "Offset lower than 7 bits: " << calcu_low << endl;
}

void vberti::prefetcher_cycle_operate() {
  current_cycle++;
}