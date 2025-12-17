#ifndef BAKSHALIPOUR_FRAMEWORK
#define BAKSHALIPOUR_FRAMEWORK

#include <vector>
#include <string>
#include <iomanip>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <random>
#include <sstream>
#include <algorithm>
using namespace std;

/**
 * A class for printing beautiful data tables.
 * It's useful for logging the information contained in tabular structures.
 */
class Table
{
public:
   Table(int width, int height) : width(width), height(height), cells(height, vector<string>(width)) {}

   void set_row(int row, const vector<string> &data, int start_col = 0)
   {
      // assert(data.size() + start_col == this->width);
      for (unsigned col = start_col; col < this->width; col += 1)
         this->set_cell(row, col, data[col]);
   }

   void set_col(int col, const vector<string> &data, int start_row = 0)
   {
      // assert(data.size() + start_row == this->height);
      for (unsigned row = start_row; row < this->height; row += 1)
         this->set_cell(row, col, data[row]);
   }

   void set_cell(int row, int col, string data)
   {
      // assert(0 <= row && row < (int)this->height);
      // assert(0 <= col && col < (int)this->width);
      this->cells[row][col] = data;
   }

   void set_cell(int row, int col, double data)
   {
      ostringstream oss;
      oss << setw(11) << fixed << setprecision(8) << data;
      this->set_cell(row, col, oss.str());
   }

   void set_cell(int row, int col, int64_t data)
   {
      ostringstream oss;
      oss << setw(11) << std::left << data;
      this->set_cell(row, col, oss.str());
   }

   void set_cell(int row, int col, int data) { this->set_cell(row, col, (int64_t)data); }

   void set_cell(int row, int col, uint64_t data)
   {
      ostringstream oss;
      oss << "0x" << setfill('0') << setw(16) << hex << data;
      this->set_cell(row, col, oss.str());
   }

   /**
    * @return The entire table as a string
    */
   string to_string()
   {
      vector<int> widths;
      for (unsigned i = 0; i < this->width; i += 1)
      {
         int max_width = 0;
         for (unsigned j = 0; j < this->height; j += 1)
            max_width = max(max_width, (int)this->cells[j][i].size());
         widths.push_back(max_width + 2);
      }
      string out;
      out += Table::top_line(widths);
      out += this->data_row(0, widths);
      for (unsigned i = 1; i < this->height; i += 1)
      {
         out += Table::mid_line(widths);
         out += this->data_row(i, widths);
      }
      out += Table::bot_line(widths);
      return out;
   }

   string data_row(int row, const vector<int> &widths)
   {
      string out;
      for (unsigned i = 0; i < this->width; i += 1)
      {
         string data = this->cells[row][i];
         data.resize(widths[i] - 2, ' ');
         out += " | " + data;
      }
      out += " |\n";
      return out;
   }

   static string top_line(const vector<int> &widths) { return Table::line(widths, "┌", "┬", "┐"); }

   static string mid_line(const vector<int> &widths) { return Table::line(widths, "├", "┼", "┤"); }

   static string bot_line(const vector<int> &widths) { return Table::line(widths, "└", "┴", "┘"); }

   static string line(const vector<int> &widths, string left, string mid, string right)
   {
      string out = " " + left;
      for (unsigned i = 0; i < widths.size(); i += 1)
      {
         int w = widths[i];
         for (int j = 0; j < w; j += 1)
            out += "─";
         if (i != widths.size() - 1)
            out += mid;
         else
            out += right;
      }
      return out + "\n";
   }

private:
   unsigned width;
   unsigned height;
   vector<vector<string>> cells;
};

template <class T>
class SetAssociativeCache
{
public:
   class Entry
   {
   public:
      uint64_t key;
      uint64_t index;
      uint64_t tag;
      bool valid;
      T data;
   };

   SetAssociativeCache(int size, int num_ways, int debug_level = 0)
       : size(size), num_ways(num_ways), num_sets(size / num_ways), entries(num_sets, vector<Entry>(num_ways)), 
       cams(num_sets), debug_level(debug_level){
      // assert(size % num_ways == 0);
      for (int i = 0; i < num_sets; i += 1)
         for (int j = 0; j < num_ways; j += 1)
            entries[i][j].valid = false;
      /* calculate `index_len` (number of bits required to store the index) */
      for (int max_index = num_sets - 1; max_index > 0; max_index >>= 1)
         this->index_len += 1;
   }

   /**
    * Invalidates the entry corresponding to the given key.
    * @return A pointer to the invalidated entry
    */
   Entry *erase(uint64_t key)
   {
      Entry *entry = this->find(key);
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      auto &cam = cams[index];
      // int num_erased = cam.erase(tag);
      cam.erase(tag);
      if (entry)
         entry->valid = false;
      // assert(entry ? num_erased == 1 : num_erased == 0);
      return entry;
   }

   /**
    * @return The old state of the entry that was updated
    */
   Entry insert(uint64_t key, const T &data)
   {
      Entry *entry = this->find(key);
      if (entry != nullptr)
      {
         Entry old_entry = *entry;
         entry->data = data;
         return old_entry;
      }
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      vector<Entry> &set = this->entries[index];
      int victim_way = -1;
      for (int i = 0; i < this->num_ways; i += 1)
         if (!set[i].valid)
         {
            victim_way = i;
            break;
         }
      if (victim_way == -1)
      {
         victim_way = this->select_victim(index);
      }
      Entry &victim = set[victim_way];
      Entry old_entry = victim;
      victim = {key, index, tag, true, data};
      auto &cam = cams[index];
      if (old_entry.valid)
      {
         // int num_erased = cam.erase(old_entry.tag);
         cam.erase(old_entry.tag);
         // assert(num_erased == 1);
      }
      cam[tag] = victim_way;
      return old_entry;
   }

   Entry *find(uint64_t key)
   {
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      auto &cam = cams[index];
      if (cam.find(tag) == cam.end())
         return nullptr;
      int way = cam[tag];
      Entry &entry = this->entries[index][way];
      // assert(entry.tag == tag && entry.valid);
      return &entry;
   }

   /**
    * Creates a table with the given headers and populates the rows by calling `write_data` on all
    * valid entries contained in the cache. This function makes it easy to visualize the contents
    * of a cache.
    * @return The constructed table as a string
    */
   string log(vector<string> headers)
   {
      vector<Entry> valid_entries = this->get_valid_entries();
      Table table(headers.size(), valid_entries.size() + 1);
      table.set_row(0, headers);
      for (unsigned i = 0; i < valid_entries.size(); i += 1)
         this->write_data(valid_entries[i], table, i + 1);
      return table.to_string();
   }

   int get_index_len() { return this->index_len; }

   void set_debug_level(int debug_level) { this->debug_level = debug_level; }

protected:
   /* should be overriden in children */
   virtual void write_data(Entry &entry, Table &table, int row) {}

   /**
    * @return The way of the selected victim
    */
   virtual int select_victim(uint64_t index)
   {
      /* random eviction policy if not overriden */
      return rand() % this->num_ways;
   }

   vector<Entry> get_valid_entries()
   {
      vector<Entry> valid_entries;
      for (int i = 0; i < num_sets; i += 1)
         for (int j = 0; j < num_ways; j += 1)
            if (entries[i][j].valid)
               valid_entries.push_back(entries[i][j]);
      return valid_entries;
   }

   int size;
   int num_ways;
   int num_sets;
   int index_len = 0; /* in bits */
   vector<vector<Entry>> entries;
   vector<unordered_map<uint64_t, int>> cams;
   int debug_level = 0;
};

template <class T>
class LRUSetAssociativeCache : public SetAssociativeCache<T>
{
   typedef SetAssociativeCache<T> Super;

public:
   LRUSetAssociativeCache(int size, int num_ways, int debug_level = 0)
       : Super(size, num_ways, debug_level), lru(this->num_sets, vector<uint64_t>(num_ways)) {}

   void set_mru(uint64_t key) { *this->get_lru(key) = this->t++; }

   void set_lru(uint64_t key) { *this->get_lru(key) = 0; }

protected:
   /* @override */
   virtual int select_victim(uint64_t index)
   {
      vector<uint64_t> &lru_set = this->lru[index];
      return min_element(lru_set.begin(), lru_set.end()) - lru_set.begin();
   }

   uint64_t *get_lru(uint64_t key)
   {
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      // assert(this->cams[index].count(tag) == 1);
      int way = this->cams[index][tag];
      return &this->lru[index][way];
   }

   vector<vector<uint64_t>> lru;
   uint64_t t = 1;
};

// template <class T>
// class FIFOSetAssociativeCache : public SetAssociativeCache<T>
// {
//    typedef SetAssociativeCache<T> Super;

// public:
//    FIFOSetAssociativeCache(int size, int num_ways, int debug_level = 0, const T &default_data = T{})
//        : Super(size, num_ways, debug_level, default_data), ptr(this->num_sets, 0) {}

//    void set_ptr(uint64_t key)
//    {
//       uint64_t index = key % this->num_sets;
//       ptr[index]++;
//       if (ptr[index] >= num_ways)
//       {
//          ptr[index] = 0;
//       }
//    }

// protected:
//    /* @override */
//    int select_victim(uint64_t index)
//    {
//       return ptr[index];
//    }
//    vector<uint16_t> ptr;
// };

uint64_t hash_index(uint64_t key, int index_len);
template <class T>
inline T square(T x) { return x * x; }


template <class T>
class GenericSatCounter
{
public:
    /** The default constructor should never be used. */
    GenericSatCounter() = delete;

    /**
     * Constructor for the counter. The explicit keyword is used to make
     * sure the user does not assign a number to the counter thinking it
     * will be used as a counter value when it is in fact used as the number
     * of bits.
     *
     * @param bits How many bits the counter will have.
     * @param initial_val Starting value for the counter.
     *
     * @ingroup api_sat_counter
     */
    explicit GenericSatCounter(unsigned bits, T initial_val = 0)
        : initialVal(initial_val), maxVal((1ULL << bits) - 1),
          counter(initial_val)
    {
        // fatal_if(bits > 8*sizeof(T),
        //  "Number of bits exceeds counter size");
        // fatal_if(initial_val > maxVal,
        //  "Saturating counter's initial value exceeds max value.");
        assert(bits <= 8 * sizeof(T));
        assert(initial_val <= maxVal);
    }

    /**
     * Copy constructor.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter(const GenericSatCounter &other)
        : initialVal(other.initialVal), maxVal(other.maxVal),
          counter(other.counter)
    {
    }

    /**
     * Copy assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &operator=(const GenericSatCounter &other)
    {
        if (this != &other)
        {
            GenericSatCounter temp(other);
            this->swap(temp);
        }
        return *this;
    }

    /**
     * Move constructor.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter(GenericSatCounter &&other)
    {
        initialVal = other.initialVal;
        maxVal = other.maxVal;
        counter = other.counter;
        GenericSatCounter temp(0);
        other.swap(temp);
    }

    /**
     * Move assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &operator=(GenericSatCounter &&other)
    {
        if (this != &other)
        {
            initialVal = other.initialVal;
            maxVal = other.maxVal;
            counter = other.counter;
            GenericSatCounter temp(0);
            other.swap(temp);
        }
        return *this;
    }

    /**
     * Swap the contents of every member of the class. Used for the default
     * copy-assignment created by the compiler.
     *
     * @param other The other object to swap contents with.
     *
     * @ingroup api_sat_counter
     */
    void
    swap(GenericSatCounter &other)
    {
        std::swap(initialVal, other.initialVal);
        std::swap(maxVal, other.maxVal);
        std::swap(counter, other.counter);
    }

    /**
     * Pre-increment operator.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator++()
    {
        if (counter < maxVal)
        {
            ++counter;
        }
        return *this;
    }

    /**
     * Post-increment operator.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter
    operator++(int)
    {
        GenericSatCounter old_counter = *this;
        ++*this;
        return old_counter;
    }

    /**
     * Pre-decrement operator.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator--()
    {
        if (counter > 0)
        {
            --counter;
        }
        return *this;
    }

    /**
     * Post-decrement operator.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter
    operator--(int)
    {
        GenericSatCounter old_counter = *this;
        --*this;
        return old_counter;
    }

    /**
     * Shift-right-assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator>>=(const int &shift)
    {
        assert(shift >= 0);
        this->counter >>= shift;
        return *this;
    }

    /**
     * Shift-left-assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator<<=(const int &shift)
    {
        assert(shift >= 0);
        this->counter <<= shift;
        if (this->counter > maxVal)
        {
            this->counter = maxVal;
        }
        return *this;
    }

    /**
     * Add-assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator+=(const long long &value)
    {
        if (value >= 0)
        {
            if (maxVal - this->counter >= value)
            {
                this->counter += value;
            }
            else
            {
                this->counter = maxVal;
            }
        }
        else
        {
            *this -= -value;
        }
        return *this;
    }

    /**
     * Subtract-assignment.
     *
     * @ingroup api_sat_counter
     */
    GenericSatCounter &
    operator-=(const long long &value)
    {
        if (value >= 0)
        {
            if (this->counter > value)
            {
                this->counter -= value;
            }
            else
            {
                this->counter = 0;
            }
        }
        else
        {
            *this += -value;
        }
        return *this;
    }

    /**
     * Read the counter's value.
     *
     * @ingroup api_sat_counter
     */
    operator T() const { return counter; }

    /**
     * Reset the counter to its initial value.
     *
     * @ingroup api_sat_counter
     */
    void reset() { counter = initialVal; }

    /**
     * Calculate saturation percentile of the current counter's value
     * with regard to its maximum possible value.
     *
     * @return A value between 0.0 and 1.0 to indicate which percentile of
     *         the maximum value the current value is.
     *
     * @ingroup api_sat_counter
     */
    double calcSaturation() const { return (double)counter / maxVal; }

    /**
     * Whether the counter has achieved its maximum value or not.
     *
     * @return True if the counter saturated.
     *
     * @ingroup api_sat_counter
     */
    bool isSaturated() const { return counter == maxVal; }

    /**
     * Saturate the counter.
     *
     * @return The value added to the counter to reach saturation.
     *
     * @ingroup api_sat_counter
     */
    T saturate()
    {
        const T diff = maxVal - counter;
        counter = maxVal;
        return diff;
    }

private:
    T initialVal;
    T maxVal;
    T counter;
};






/** @ingroup api_sat_counter
 *  @{
 */
typedef GenericSatCounter<uint8_t> SatCounter8;
typedef GenericSatCounter<uint16_t> SatCounter16;
typedef GenericSatCounter<uint32_t> SatCounter32;
typedef GenericSatCounter<uint64_t> SatCounter64;





class TaggedEntry
{
public:
    TaggedEntry() : _valid(false), _secure(false), _tag((uint64_t)-1) {}
    ~TaggedEntry() = default;

    /**
     * Checks if the entry is valid.
     *
     * @return True if the entry is valid.
     */
    virtual bool isValid() const { return _valid; }

    /**
     * Check if this block holds data from the secure memory space.
     *
     * @return True if the block holds data from the secure memory space.
     */
    bool isSecure() const { return _secure; }

    /**
     * Get tag associated to this block.
     *
     * @return The tag value.
     */
    virtual uint64_t getTag() const { return _tag; }

    /**
     * Checks if the given tag information corresponds to this entry's.
     *
     * @param tag The tag value to compare to.
     * @param is_secure Whether secure bit is set.
     * @return True if the tag information match this entry's.
     */
    virtual bool
    matchTag(uint64_t tag, bool is_secure) const
    {
        return isValid() && (getTag() == tag) && (isSecure() == is_secure);
    }

    /**
     * Insert the block by assigning it a tag and marking it valid. Touches
     * block if it hadn't been touched previously.
     *
     * @param tag The tag value.
     */
    virtual void
    insert(const uint64_t tag, const bool is_secure)
    {
        setValid();
        setTag(tag);
        if (is_secure)
        {
            setSecure();
        }
    }

    /** Invalidate the block. Its contents are no longer valid. */
    virtual void invalidate()
    {
        _valid = false;
        setTag((uint64_t)-1);
        clearSecure();
    }

    // std::string
    // print() const override
    // {
    //     return csprintf("tag: %#x secure: %d valid: %d | %s", getTag(),
    //         isSecure(), isValid(), ReplaceableEntry::print());
    // }

protected:
    /**
     * Set tag associated to this block.
     *
     * @param tag The tag value.
     */
    virtual void setTag(uint64_t tag) { _tag = tag; }

    /** Set secure bit. */
    virtual void setSecure() { _secure = true; }

    /** Set valid bit. The block must be invalid beforehand. */
    virtual void
    setValid()
    {
        assert(!isValid());
        _valid = true;
    }

private:
    /**
     * Valid bit. The contents of this entry are only valid if this bit is set.
     * @sa invalidate()
     * @sa insert()
     */
    bool _valid;

    /**
     * Secure bit. Marks whether this entry refers to an address in the secure
     * memory space. Must always be modified along with the tag.
     */
    bool _secure;

    /** The entry's tag. */
    uint64_t _tag;

    /** Clear secure bit. Should be only used by the invalidation function. */
    void clearSecure() { _secure = false; }
};



#endif /* BAKSHALIPOUR_FRAMEWORK */
