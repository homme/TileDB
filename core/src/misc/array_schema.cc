/**
 * @file   array_schema.cc
 * @author Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * @section LICENSE
 *
 * The MIT License
 * 
 * @copyright Copyright (c) 2014 Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * @section DESCRIPTION
 *
 * This file implements the ArraySchema class.
 */

#include "array_schema.h"
#include "hilbert_curve.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <set>

/******************************************************
************ CONSTRUCTORS & DESTRUCTORS ***************
******************************************************/

ArraySchema::ArraySchema() {
}

ArraySchema::ArraySchema(
    const std::string& array_name,
    const std::vector<std::string>& attribute_names,
    const std::vector<std::string>& dim_names,
    const std::vector<std::pair<double, double> >& dim_domains,
    const std::vector<const std::type_info*>& types,
    const std::vector<int>& val_num,
    CellOrder cell_order,
    int consolidation_step,
    int64_t capacity) {
  assert(attribute_names.size() > 0);
  assert(dim_names.size() > 0);
  assert(attribute_names.size()+1 == types.size());
  assert(dim_names.size() == dim_domains.size());
  assert(capacity > 0);
  assert(consolidation_step > 0);
  assert(val_num.size() == attribute_names.size());
#ifndef NDEBUG
  for(int i=0; i<dim_domains.size(); ++i) 
    assert(dim_domains[i].first <= dim_domains[i].second);
#endif
 
  array_name_ = array_name;
  attribute_names_ = attribute_names;
  dim_names_ = dim_names;
  dim_domains_ = dim_domains;
  types_ = types;
  val_num_ = val_num;
  cell_order_ = cell_order;
  tile_order_ = TO_NONE;
  consolidation_step_ = consolidation_step;
  capacity_ = capacity;
  dim_num_ = dim_names_.size();
  attribute_num_ = attribute_names_.size();
  // Name for the extra coordinate attribute
  attribute_names_.push_back(AS_COORDINATE_TILE_NAME);

  // Set cell sizes
  cell_size_ = 0;
  cell_sizes_.resize(attribute_num_+1);
  for(int i=0; i<= attribute_num_; ++i) {
    cell_sizes_[i] = compute_cell_size(i);
    if(cell_sizes_[i] == VAR_SIZE)
      cell_size_ = VAR_SIZE;
    if(cell_size_ != VAR_SIZE)
      cell_size_ += cell_sizes_[i]; 
  }

  // Set type sizes
  type_sizes_.resize(attribute_num_ + 1);
  for(int i=0; i<=attribute_num_; ++i)
    type_sizes_[i] = compute_type_size(i);

  // Set compression
  for(int i=0; i<= attribute_num_; ++i)
    compression_.push_back(NONE);

  compute_hilbert_cell_bits();
}

ArraySchema::ArraySchema(
    const std::string& array_name,
    const std::vector<std::string>& attribute_names,
    const std::vector<std::string>& dim_names,
    const std::vector<std::pair<double, double> >& dim_domains,
    const std::vector<const std::type_info*>& types,
    const std::vector<int>& val_num,
    TileOrder tile_order,
    const std::vector<double>& tile_extents,
    int consolidation_step,
    int64_t capacity, 
    CellOrder cell_order) {
  assert(attribute_names.size() > 0);
  assert(dim_names.size() > 0);
  assert(tile_extents.size() > 0);
  assert(attribute_names.size()+1 == types.size());
  assert(dim_names.size() == dim_domains.size());
  assert(dim_names.size() == tile_extents.size());
  assert(capacity > 0);
  assert(consolidation_step > 0);
  assert(val_num.size() == attribute_names.size());
#ifndef NDEBUG
  for(int i=0; i<dim_domains.size(); ++i) 
    assert(dim_domains[i].first <= dim_domains[i].second);
  for(int i=0; i<tile_extents.size(); ++i) {
    assert(tile_extents[i] != 0);
    assert(tile_extents[i] <= (dim_domains[i].second - 
                               dim_domains[i].first + 1));
  }
#endif

  array_name_ = array_name;
  attribute_names_ = attribute_names;
  dim_names_ = dim_names;
  dim_domains_ = dim_domains;
  types_ = types;
  val_num_ = val_num;
  tile_order_ = tile_order;
  cell_order_ = cell_order;
  consolidation_step_ = consolidation_step;
  capacity_ = capacity;
  tile_extents_ = tile_extents; 
  dim_num_ = dim_names_.size();
  attribute_num_ = attribute_names_.size();
  // Name for the extra coordinate attribute
  attribute_names_.push_back(AS_COORDINATE_TILE_NAME); 

  // Set cell sizes
  cell_size_ = 0;
  cell_sizes_.resize(attribute_num_+1);
  for(int i=0; i<= attribute_num_; ++i) {
    cell_sizes_[i] = compute_cell_size(i);
    if(cell_sizes_[i] == VAR_SIZE)
      cell_size_ = VAR_SIZE;
    if(cell_size_ != VAR_SIZE)
      cell_size_ += cell_sizes_[i]; 
  }

  // Set type sizes
  type_sizes_.resize(attribute_num_ + 1);
  for(int i=0; i<=attribute_num_; ++i)
    type_sizes_[i] = compute_type_size(i);

  // Set compression
  for(int i=0; i<= attribute_num_; ++i)
    compression_.push_back(NONE);

  compute_hilbert_cell_bits();
  compute_hilbert_tile_bits();
  compute_tile_id_offsets();
}

ArraySchema::~ArraySchema() {
}

/******************************************************
********************** ACCESSORS **********************
******************************************************/

const std::string& ArraySchema::array_name() const {
  return array_name_;
}

int ArraySchema::attribute_id(const std::string& attribute_name) const {
  for(int i=0; i<attribute_num_; ++i) {
    if(attribute_names_[i] == attribute_name)
      return i;
  }

  // Attribute not found
  return -1;
}

std::vector<int> ArraySchema::attribute_ids() const {
  std::vector<int> attribute_ids;

  for(int i=0; i<=attribute_num_; ++i)
    attribute_ids.push_back(i);

  return attribute_ids;
}

const std::string& ArraySchema::attribute_name(int i) const {
  assert(i>= 0 && i <= attribute_num_);

  return attribute_names_[i];
}

int ArraySchema::attribute_num() const {
  return attribute_num_;
}

int64_t ArraySchema::capacity() const {
  return capacity_;
}

ArraySchema::CellOrder ArraySchema::cell_order() const {
  return cell_order_;
}

size_t ArraySchema::cell_size() const {
  return cell_size_;
}

size_t ArraySchema::cell_size(int i) const {
  return cell_sizes_[i];
}

size_t ArraySchema::cell_size(const std::vector<int>& attribute_ids) const {
  assert(valid_attribute_ids(attribute_ids));
  assert(no_duplicates(attribute_ids));

  if(attribute_ids.size() == attribute_num_ + 1)
    return cell_size_;

  size_t cell_size = 0;
  for(int i=0; i<attribute_ids.size(); ++i) {
    if(cell_sizes_[attribute_ids[i]] == VAR_SIZE)
      return VAR_SIZE;
    cell_size += cell_sizes_[attribute_ids[i]];
  }

  return cell_size;
}

size_t ArraySchema::coords_size() const {
  return cell_sizes_[attribute_num_];
}

const std::type_info* ArraySchema::coords_type() const {
  return type(attribute_num_);
}

int ArraySchema::consolidation_step() const {
  return consolidation_step_;
}

const std::vector<std::pair<double,double> >& ArraySchema::dim_domains() const {
  return dim_domains_;
}

int ArraySchema::dim_id(const std::string& dim_name) const {
  for(int i=0; i<dim_num_; ++i) {
    if(dim_names_[i] == dim_name)
      return i;
  }

  // Dimension not found
  return -1;
}

int ArraySchema::dim_num() const {
  return dim_num_;
}

// FORMAT:
// array_name_size(int) array_name(string)
// tile_order(char)
// cell_order(char)
// capacity(int64_t)
// consolidation_step(int)
// attribute_num(int) 
//     attribute_name_size#1(int) attribute_name#1(string)
//     attribute_name_size#2(int) attribute_name#2(string) 
//     ...
// dim_num(int) 
//    dim_name_size#1(int) dim_name#1(string)
//    dim_name_size#2(int) dim_name#2(string)
//    ...
// dim_domain#1_low(double) dim_domain#1_high(double)
// dim_domain#2_low(double) dim_domain#2_high(double)
//  ...
// tile_extents_num(int) 
//     tile_extent#1(double) tile_extent#2(double) ... 
// type#1(char) type#2(char) ... 
// val_num#1(int) val_num#2(int) ... 
// compression#1(char) compression#2(char) ...
std::pair<const char*, size_t> ArraySchema::serialize() const {
  size_t buffer_size = 0;

  // ====== Calculation of buffer_size ======
  // Size for array_name_ 
  buffer_size += sizeof(int) + array_name_.size();
  // Size for tile_order_ and cell_order_
  buffer_size += 2 * sizeof(char);
  // Size for capacity_ 
  buffer_size += sizeof(int64_t);
  // Size for consolidation_step__ 
  buffer_size += sizeof(int);
  // Size for attribute_names_ 
  buffer_size += sizeof(int);
  for(int i=0; i<attribute_num_; ++i)
    buffer_size += sizeof(int) + attribute_names_[i].size();
  // Size for dim_names_
  buffer_size += sizeof(int);
  for(int i=0; i<dim_num_; ++i)
    buffer_size += sizeof(int) + dim_names_[i].size();
  // Size for dim_domains_
  buffer_size += 2 * dim_num_ * sizeof(double);
  // Size for tile_extents_ 
  // (recall that an array with irregular tiles does not have tile extents)
  buffer_size += sizeof(int) + tile_extents_.size() * sizeof(double);
  // Size for types_
  buffer_size += (attribute_num_+1) * sizeof(char);
  // Size for val_num_
  buffer_size += attribute_num_ * sizeof(int);
  // Size for compression_
  buffer_size += (attribute_num_+1) * sizeof(char);

  char* buffer = new char[buffer_size];

  // ====== Populating the buffer ======
  size_t offset = 0;
  // Copy array_name_
  int array_name_size = array_name_.size();
  assert(offset < buffer_size);
  memcpy(buffer + offset, &array_name_size, sizeof(int));
  offset += sizeof(int);
  assert(offset < buffer_size);
  memcpy(buffer + offset, array_name_.c_str(), array_name_size);
  offset += array_name_size;
  // Copy tile_order_ and cell_order_
  char tile_order = tile_order_;
  assert(offset < buffer_size);
  memcpy(buffer + offset, &tile_order, sizeof(char));
  offset += sizeof(char);
  char cell_order = cell_order_;
  assert(offset < buffer_size);
  memcpy(buffer + offset, &cell_order, sizeof(char));
  offset += sizeof(char);
  // Copy capacity_
  assert(offset < buffer_size);
  memcpy(buffer + offset, &capacity_, sizeof(int64_t));
  offset += sizeof(int64_t);
  // Copy consolidation_step_
  assert(offset < buffer_size);
  memcpy(buffer + offset, &consolidation_step_, sizeof(int));
  offset += sizeof(int);
  // Copy attribute_names_
  assert(offset < buffer_size);
  memcpy(buffer + offset, &attribute_num_, sizeof(int));
  offset += sizeof(unsigned int);
  int attribute_name_size;
  for(int i=0; i<attribute_num_; i++) {
    attribute_name_size = attribute_names_[i].size();
    assert(offset < buffer_size);
    memcpy(buffer + offset, &attribute_name_size, sizeof(int)); 
    offset += sizeof(int);
    assert(offset < buffer_size);
    memcpy(buffer + offset, attribute_names_[i].c_str(), attribute_name_size); 
    offset += attribute_name_size;
  }
  // Copy dim_names_
  assert(offset < buffer_size);
  memcpy(buffer + offset, &dim_num_, sizeof(int));
  offset += sizeof(int);
  int dim_name_size;
  for(int i=0; i<dim_num_; i++) {
    dim_name_size = dim_names_[i].size();
    assert(offset < buffer_size);
    memcpy(buffer + offset, &dim_name_size, sizeof(int)); 
    offset += sizeof(int);
    assert(offset < buffer_size);
    memcpy(buffer + offset, dim_names_[i].c_str(), dim_name_size); 
    offset += dim_name_size;
  }
  // Copy dim_domains_
  for(int i=0; i<dim_num_; i++) {
    assert(offset < buffer_size);
    memcpy(buffer + offset, &dim_domains_[i].first, sizeof(double));
    offset += sizeof(double);
    assert(offset < buffer_size);
    memcpy(buffer + offset, &dim_domains_[i].second, sizeof(double));
    offset += sizeof(double);
  } 
  // Copy tile_extents_
  int tile_extents_num = tile_extents_.size();
  assert(offset < buffer_size);
  memcpy(buffer + offset, &tile_extents_num, sizeof(int));
  offset += sizeof(int);
  for(int i=0; i<tile_extents_num; i++) {
    assert(offset < buffer_size);
    memcpy(buffer + offset, &tile_extents_[i], sizeof(double));
    offset += sizeof(double);
  }
  // Copy types_
  char type; 
  for(int i=0; i<=attribute_num_; i++) {
    if(*types_[i] == typeid(char))
      type = CHAR;
    else if(*types_[i] == typeid(int))
      type = INT;
    else if(*types_[i] == typeid(int64_t))
      type = INT64_T;
    else if(*types_[i] == typeid(float))
      type = FLOAT;
    else if(*types_[i] == typeid(double))
      type = DOUBLE;
    assert(offset < buffer_size);
    memcpy(buffer + offset, &type, sizeof(char));
    offset += sizeof(char);
  }
  // Copy val_num_
  for(int i=0; i<attribute_num_; i++) {
    memcpy(buffer + offset, &val_num_[i], sizeof(int));
    offset += sizeof(int);
  }
  // Copy compression_
  char compression; 
  for(int i=0; i<=attribute_num_; ++i) {
    compression = compression_[i];
    assert(offset < buffer_size);
    memcpy(buffer + offset, &compression, sizeof(char));
    offset += sizeof(char);
  }
  assert(offset == buffer_size);

  return std::pair<char*, size_t>(buffer, buffer_size);
}

int ArraySchema::smallest_attribute() const {
  int smallest_attribute = 0;
  size_t smallest_cell_size = this->cell_size(0);
  size_t smallest_type_size = this->type_size(0);
  size_t cell_size, type_size;

  // Check for smallest cell size
  for(int i=1; i<attribute_num_; ++i) {
    cell_size = this->cell_size(i);
    if(cell_size != VAR_SIZE && 
       (smallest_cell_size == VAR_SIZE || cell_size < smallest_cell_size)) {
      smallest_cell_size = cell_size;
      smallest_attribute = i; 
    }
  }

  // If all cell sizes are varible, choose the smallest type
  if(smallest_cell_size == VAR_SIZE) {  
    assert(smallest_attribute == 0);
    for(int i=1; i<attribute_num_; ++i) {
      type_size = this->type_size(i);
      if(type_size < smallest_type_size) {
        smallest_type_size = type_size;
        smallest_attribute = i; 
      }
    }
  }

  return smallest_attribute;
}

const std::vector<double>& ArraySchema::tile_extents() const {
  return tile_extents_;
}

ArraySchema::TileOrder ArraySchema::tile_order() const {
  return tile_order_;
}

const std::type_info* ArraySchema::type(int i) const {
  assert(i>=0 && i<=attribute_num_);

  return types_[i];
}

size_t ArraySchema::type_size(int i) const {
  assert(i>=0 && i<=attribute_num_);

  return type_sizes_[i];
}

int ArraySchema::val_num(int attribute_id) const {
  assert(attribute_id >=0 && attribute_id < attribute_num_);

  return val_num_[attribute_id];
}

/******************************************************
*********************** MUTATORS **********************
******************************************************/

// FORMAT:
// array_name_size(int) array_name(string)
// tile_order(char)
// cell_order(char)
// capacity(int64_t)
// consolidation_step(int)
// attribute_num(int) 
//     attribute_name_size#1(int) attribute_name#1(string)
//     attribute_name_size#2(int) attribute_name#2(string) 
//     ...
// dim_num(int) 
//    dim_name_size#1(int) dim_name#1(string)
//    dim_name_size#2(int) dim_name#2(string)
//    ...
// dim_domain#1_low(double) dim_domain#1_high(double)
// dim_domain#2_low(double) dim_domain#2_high(double)
//  ...
// tile_extents_num(int) 
//     tile_extent#1(double) tile_extent#2(double) ... 
// type#1(char) type#2(char) ... 
// val_num#1(int) val_num#2(int) ... 
// compression#1(char) compression#2(char) ...
void ArraySchema::deserialize(const char* buffer, size_t buffer_size) {
  size_t offset = 0;

  // Load array_name_ 
  int array_name_size;
  assert(offset < buffer_size);
  memcpy(&array_name_size, buffer + offset, sizeof(int));
  offset += sizeof(int);
  array_name_.resize(array_name_size);
  assert(offset < buffer_size);
  memcpy(&array_name_[0], buffer + offset, array_name_size);
  offset += array_name_size;
  // Load tile_order_ and cell_order_
  char tile_order;
  assert(offset < buffer_size);
  memcpy(&tile_order, buffer + offset, sizeof(char));
  tile_order_ = static_cast<TileOrder>(tile_order);  
  offset += sizeof(char);
  char cell_order;
  assert(offset < buffer_size);
  memcpy(&cell_order, buffer + offset, sizeof(char));
  cell_order_ = static_cast<CellOrder>(cell_order);  
  offset += sizeof(char);
  // Load capacity_
  assert(offset < buffer_size);
  memcpy(&capacity_, buffer + offset, sizeof(int64_t));
  offset += sizeof(int64_t);
  // Load consolidation_step_
  assert(offset < buffer_size);
  memcpy(&consolidation_step_, buffer + offset, sizeof(int));
  offset += sizeof(int);
  // Load attribute_names_
  assert(offset < buffer_size);
  memcpy(&attribute_num_, buffer + offset, sizeof(int));
  offset += sizeof(int);
  attribute_names_.resize(attribute_num_);
  int attribute_name_size;
  for(int i=0; i<attribute_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&attribute_name_size, buffer+offset, sizeof(int)); 
    offset += sizeof(int);
    attribute_names_[i].resize(attribute_name_size);
    assert(offset < buffer_size);
    memcpy(&attribute_names_[i][0], 
           buffer + offset, attribute_name_size);
    offset += attribute_name_size;
  }
  // Load dim_names_
  assert(offset < buffer_size);
  memcpy(&dim_num_, buffer + offset, sizeof(int));
  offset += sizeof(int);
  dim_names_.resize(dim_num_);
  int dim_name_size;
  for(int i=0; i<dim_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&dim_name_size, buffer + offset, sizeof(int)); 
    offset += sizeof(int);
    dim_names_[i].resize(dim_name_size);
    assert(offset < buffer_size);
    memcpy(&dim_names_[i][0], buffer + offset, dim_name_size); 
    offset += dim_name_size;
  }
  // Load dim_domains
  dim_domains_.resize(dim_num_);
  for(int i=0; i<dim_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&dim_domains_[i].first, buffer + offset, sizeof(double));
    offset += sizeof(double);
    assert(offset < buffer_size);
    memcpy(&dim_domains_[i].second, buffer + offset, sizeof(double));
    offset += sizeof(double);
  } 
  // Load tile_extents_
  int tile_extents_num;
  assert(offset < buffer_size);
  memcpy(&tile_extents_num, buffer + offset, sizeof(int));
  offset += sizeof(int);
  tile_extents_.resize(tile_extents_num);
  for(int i=0; i<tile_extents_num; ++i) {
    assert(offset < buffer_size);
    memcpy(&tile_extents_[i], buffer + offset, sizeof(double));
    offset += sizeof(double);
  }
  // Load types_
  char type;
  types_.resize(attribute_num_+1); 
  type_sizes_.resize(attribute_num_+1);
  for(int i=0; i<=attribute_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&type, buffer + offset, sizeof(char));
    offset += sizeof(char);
    if(type == CHAR) {
      types_[i] = &typeid(char);
      type_sizes_[i] = sizeof(char);
    } else if(type == INT) {
      types_[i] = &typeid(int);
      type_sizes_[i] = sizeof(int);
    } else if(type == INT64_T) {
      types_[i] = &typeid(int64_t);
      type_sizes_[i] = sizeof(int64_t);
    } else if(type == FLOAT) {
      types_[i] = &typeid(float);
      type_sizes_[i] = sizeof(float);
    } else if(type == DOUBLE) {
      types_[i] = &typeid(double);
      type_sizes_[i] = sizeof(double);
    }
  }
  // Load val_num_
  val_num_.resize(attribute_num_); 
  for(int i=0; i<attribute_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&val_num_[i], buffer + offset, sizeof(int));
    offset += sizeof(int);
  }
  // Load compression_
  char compression;
  for(int i=0; i<=attribute_num_; ++i) {
    assert(offset < buffer_size);
    memcpy(&compression, buffer + offset, sizeof(char));
    offset += sizeof(char);
    compression_.push_back(static_cast<CompressionType>(compression));
  }
  assert(offset == buffer_size);
 
  // Extra coordinate attribute
  attribute_names_.push_back(AS_COORDINATE_TILE_NAME);

  // Set cell sizes
  cell_size_ = 0;
  cell_sizes_.resize(attribute_num_+1);
  for(int i=0; i<= attribute_num_; ++i) {
    cell_sizes_[i] = compute_cell_size(i);
    if(cell_sizes_[i] == VAR_SIZE)
      cell_size_ = VAR_SIZE;
    if(cell_size_ != VAR_SIZE)
      cell_size_ += cell_sizes_[i]; 
  }

  compute_hilbert_cell_bits();
  if(tile_extents_.size() != 0) { // Only for regular tiles
    compute_hilbert_tile_bits();
    compute_tile_id_offsets();
  }
}

/******************************************************
************************ MISC *************************
******************************************************/

int64_t ArraySchema::cell_id_hilbert(const void* coords) const {
  if(*(types_[attribute_num_]) == typeid(int))
    return cell_id_hilbert(static_cast<const int*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(int64_t))
    return cell_id_hilbert(static_cast<const int64_t*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(float))
    return cell_id_hilbert(static_cast<const float*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(double))
    return cell_id_hilbert(static_cast<const double*>(coords));  
}

template<typename T>
int64_t ArraySchema::cell_id_hilbert(const T* coordinates) const {
  assert(*types_[attribute_num_] == typeid(T));
#ifndef NDEBUG
  for(int i=0; i<dim_num_; ++i) 
    assert(coordinates[i] >= dim_domains_[i].first &&
           coordinates[i] <= dim_domains_[i].second);
#endif 

  bool regular = (tile_extents_.size() != 0);

  HilbertCurve *hc = new HilbertCurve();
  int *coord = new int[dim_num_];
 
  if(regular) {
    for(int i = 0; i < dim_num_; ++i) 
      coord[i] = int(coordinates[i]) % int(tile_extents_[i]);
  } else { 
    for(int i = 0; i < dim_num_; ++i) 
      coord[i] = int(coordinates[i]);
  }

  int64_t cell_ID = hc->AxestoLine(coord, hilbert_cell_bits_, dim_num_);	

  delete hc;
  delete [] coord;
	
  return cell_ID;
}

ArraySchema* ArraySchema::clone(const std::string& array_name) const {
  ArraySchema* array_schema = new ArraySchema();
  *array_schema = *this;
  array_schema->array_name_ = array_name; // Input array name

  return array_schema;
}

ArraySchema* ArraySchema::clone(const std::string& array_name,
                                const std::vector<int>& attribute_ids) const {
  assert(valid_attribute_ids(attribute_ids));

  ArraySchema* array_schema = new ArraySchema();
  *array_schema = *this;

  // Change array name
  array_schema->array_name_ = array_name; // Input array name

  // Change attribute names
  array_schema->attribute_names_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->attribute_names_.push_back(attribute_name(attribute_ids[i]));
  // Name for the extra coordinate attribute
  array_schema->attribute_names_.push_back(AS_COORDINATE_TILE_NAME);
 
  // Change attribute_num_
  array_schema->attribute_num_ = attribute_ids.size();

  // Change cell sizes
  array_schema->cell_sizes_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->cell_sizes_.push_back(cell_sizes_[attribute_ids[i]]);
  array_schema->cell_sizes_.push_back(cell_sizes_.back());
 
  // Change types
  array_schema->types_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->types_.push_back(types_[attribute_ids[i]]);
  array_schema->types_.push_back(types_.back());

  // Change type sizes
  array_schema->type_sizes_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->type_sizes_.push_back(type_sizes_[attribute_ids[i]]);
  array_schema->type_sizes_.push_back(type_sizes_.back());

  // Change val num
  array_schema->val_num_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->val_num_.push_back(val_num_[attribute_ids[i]]);
  array_schema->val_num_.push_back(val_num_.back());

  // Change compression
  array_schema->compression_.clear();
  for(int i=0; i<attribute_ids.size(); ++i) 
    array_schema->compression_.push_back(compression_[attribute_ids[i]]);
  array_schema->compression_.push_back(compression_.back());
 
  return array_schema;
}

ArraySchema* ArraySchema::clone(
    const std::string& array_name, CellOrder cell_order) const {
  ArraySchema* array_schema = new ArraySchema();
  *array_schema = *this;
  array_schema->array_name_ = array_name; // Input array name
  array_schema->cell_order_ = cell_order;  // Input cell order

  return array_schema;
}

ArraySchema* ArraySchema::clone(int64_t capacity) const {
  ArraySchema* array_schema = new ArraySchema();
  *array_schema = *this;
  array_schema->capacity_ = capacity; // Input capacity

  return array_schema;
}

ArraySchema ArraySchema::create_join_result_schema(
    const ArraySchema& array_schema_A, 
    const ArraySchema& array_schema_B,
    const std::string& result_array_name) {
  // Attribute names
  std::vector<std::string> join_attribute_names;
  join_attribute_names.insert(join_attribute_names.end(),
                              array_schema_A.attribute_names_.begin(),
                              --array_schema_A.attribute_names_.end());
  std::set<std::string> attribute_names_A_set(
                            array_schema_A.attribute_names_.begin(),
                            --array_schema_A.attribute_names_.end());
  join_attribute_names.insert(join_attribute_names.end(),
                              array_schema_B.attribute_names_.begin(),
                              --array_schema_B.attribute_names_.end());
  for(int i=array_schema_A.attribute_num_; 
      i<join_attribute_names.size(); ++i) {
    if(attribute_names_A_set.find(join_attribute_names[i]) != 
       attribute_names_A_set.end())
      join_attribute_names[i] += "_2";
  }

  // Types
  std::vector<const std::type_info*> join_types;
  join_types.insert(join_types.end(),
                    array_schema_A.types_.begin(),
                    --array_schema_A.types_.end());
  join_types.insert(join_types.end(),
                    array_schema_B.types_.begin(),
                    array_schema_B.types_.end());

  // Number of values per attribute
  std::vector<int> join_val_num;
  join_val_num.insert(join_val_num.end(),
                      array_schema_A.val_num_.begin(),
                      array_schema_A.val_num_.end());
  join_val_num.insert(join_val_num.end(),
                      array_schema_B.val_num_.begin(),
                      array_schema_B.val_num_.end());

  // Irregular tiles
  if(array_schema_A.has_irregular_tiles())
    return ArraySchema(result_array_name, join_attribute_names, 
                       array_schema_A.dim_names_, 
                       array_schema_A.dim_domains_, 
                       join_types, join_val_num,
                       array_schema_A.cell_order_, 
                       array_schema_A.consolidation_step_, 
                       array_schema_A.capacity_);
  else // Regular tiles
    return ArraySchema(result_array_name, join_attribute_names, 
                       array_schema_A.dim_names_,
                       array_schema_A.dim_domains_, 
                       join_types, join_val_num,
                       array_schema_A.tile_order_,
                       array_schema_A.tile_extents_,
                       array_schema_A.consolidation_step_, 
                       array_schema_A.capacity_,
                       array_schema_A.cell_order_);
} 

bool ArraySchema::has_irregular_tiles() const {
  return (tile_extents_.size() == 0);
}

bool ArraySchema::has_regular_tiles() const {
  return (tile_extents_.size() != 0);
}

std::pair<bool,std::string> ArraySchema::join_compatible(
    const ArraySchema& array_schema_A,
    const ArraySchema& array_schema_B) {
  // Tile type 
  if(array_schema_A.has_regular_tiles() != array_schema_B.has_regular_tiles())
    return std::pair<bool,std::string>(false,"Tile type mismatch.");

  // Number of dimensions
  if(array_schema_A.dim_num_ != array_schema_B.dim_num_)
    return std::pair<bool,std::string>(false,"Dimension number mismatch.");

  // Cell type of dimensions
  if(*(array_schema_A.types_[array_schema_A.attribute_num_]) != 
     *(array_schema_B.types_[array_schema_B.attribute_num_]))
    return std::pair<bool,std::string>(false,"Dimension type mismatch.");

  // Domains
  for(int i=0; i<array_schema_A.dim_num_; ++i)
    if(array_schema_A.dim_domains_[i].first != 
       array_schema_B.dim_domains_[i].first || 
       array_schema_A.dim_domains_[i].second != 
       array_schema_B.dim_domains_[i].second)
      return std::pair<bool,std::string>(false,"Domain mismatch.");

  // Orders
  if(array_schema_A.tile_order_ != array_schema_B.tile_order_)
    return std::pair<bool,std::string>(false,"Tile order mismatch.");
  if(array_schema_A.cell_order_ != array_schema_B.cell_order_)
    return std::pair<bool,std::string>(false,"Cell order mismatch.");

  // Tile extents
  for(int i=0; i<array_schema_A.tile_extents_.size(); ++i)
    if(array_schema_A.tile_extents_[i] != array_schema_B.tile_extents_[i])
      return std::pair<bool,std::string>(false,"Tile extent mismatch.");

  return std::pair<bool,std::string>(true,"");
}


bool ArraySchema::precedes(const void* coords_A,
                           const void* coords_B) const {
  if(*types_[attribute_num_] == typeid(int)) {
    return precedes(static_cast<const int*>(coords_A), 
                    static_cast<const int*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(int64_t)) {
    return precedes(static_cast<const int64_t*>(coords_A), 
                    static_cast<const int64_t*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(float)) {
    return precedes(static_cast<const float*>(coords_A), 
                    static_cast<const float*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(double)) {
    return precedes(static_cast<const double*>(coords_A), 
                    static_cast<const double*>(coords_B));
  } else {
    assert(0); // The code must never reach this point
  }
}

template<class T>
bool ArraySchema::precedes(const T* coords_A,
                           const T* coords_B) const {
  assert(*types_[attribute_num_] == typeid(T));

  // ROW_MAJOR order
  if(cell_order_ == CO_ROW_MAJOR) {
    for(int i=0; i<dim_num_; ++i) {
      if(coords_A[i] < coords_B[i])
        return true;
      else if(coords_A[i] > coords_B[i])
        return false;
      // else coordinate is equal - check the next dimension
    }
    return false; // Coordinates are equal
  // COLUMN_MAJOR order
  } else if(cell_order_ == CO_COLUMN_MAJOR) {
    for(int i=dim_num_-1; i>=0; i--) {
      if(coords_A[i] < coords_B[i])
        return true;
      else if(coords_A[i] > coords_B[i])
        return false;
      // else coordinate is equal - check the next dimension
    }
    return false; // Coordinates are equal
  // HILBERT order
  } else if(cell_order_ == CO_HILBERT) {
    int64_t cell_id_A = cell_id_hilbert(coords_A);
    int64_t cell_id_B = cell_id_hilbert(coords_B);
    if(cell_id_A < cell_id_B) {
      return true;
    } else if (cell_id_A > cell_id_B) {
      return false;
    } else { // (cell_id_A == cell_id_B): check ROW_MAJOR order
      for(int i=0; i<dim_num_; ++i) {
        if(coords_A[i] < coords_B[i])
          return true;
        else if(coords_A[i] > coords_B[i])
          return false;
        // else coordinate is equal - check the next dimension
      }
      return false; // Coordinates are equal
    }
  } else { // it should never reach this point
    assert(0);
  }
}

void ArraySchema::print() const {
  std::cout << "Array name: " << array_name_ << "\n";
  if(has_regular_tiles()) {
    std::cout << "Tile order: ";
    if(tile_order_ == TO_COLUMN_MAJOR)
      std::cout << "COLUMN MAJOR\n";
    else if(tile_order_ == TO_HILBERT)
      std::cout << "HILBERT\n";
    else if(tile_order_ == TO_ROW_MAJOR)
      std::cout << "ROW_MAJOR\n";
    else if(tile_order_ == TO_NONE)
      std::cout << "NONE\n";
  }
  std::cout << "Cell order: ";
  if(cell_order_ == CO_COLUMN_MAJOR)
    std::cout << "COLUMN MAJOR\n";
  else if(cell_order_ == CO_HILBERT)
    std::cout << "HILBERT\n";
  else if(cell_order_ == CO_ROW_MAJOR)
    std::cout << "ROW_MAJOR\n";
  else if(cell_order_ == CO_NONE)
    std::cout << "NONE\n";

  std::cout << "Capacity: " << capacity_ << "\n";
  std::cout << "Consolidation step: " << consolidation_step_ << "\n";

  std::cout << "Attribute num: " << attribute_num_ << "\n";
  std::cout << "Attribute names:\n";
  for(int i=0; i<attribute_num_; ++i)
    std::cout << "\t" << attribute_names_[i] << "\n";
  std::cout << "Dimension num: " << dim_num_ << "\n";
  std::cout << "Dimension names:\n";
  for(int i=0; i<dim_num_; ++i)
    std::cout << "\t" << dim_names_[i] << "\n";
  std::cout << "Dimension domains:\n";
  for(int i=0; i<dim_num_; ++i)
    std::cout << "\t[" << dim_domains_[i].first << "," 
                        << dim_domains_[i].second << "]\n";
  std::cout << (has_regular_tiles() ? "Regular" : "Irregular") << " tiles\n";
  if(has_regular_tiles()) {
    std::cout << "Tile extents:\n";
    for(int i=0; i<dim_num_; ++i)
      std::cout << "\t" << tile_extents_[i] << "\n";
  }

  std::cout << "Cell types:\n";
  for(int i=0; i<attribute_num_; ++i) {
    if(*types_[i] == typeid(char)) {
      std::cout << "\t" << attribute_names_[i] << ": char[";
    } else if(*types_[i] == typeid(int)) {
      std::cout << "\t" << attribute_names_[i] << ": int[";
    } else if(*types_[i] == typeid(int64_t)) {
      std::cout << "\t" << attribute_names_[i] << ": int64_t[";
    } else if(*types_[i] == typeid(float)) {
      std::cout << "\t" << attribute_names_[i] << ": float[";
    } else if(*types_[i] == typeid(double)) {
      std::cout << "\t" << attribute_names_[i] << ": double[";
    }
    if(val_num_[i] == VAR_SIZE)
      std::cout << "var]\n";
    else
      std::cout << val_num_[i] << "]\n";
  }
  if(*types_[attribute_num_] == typeid(int))
    std::cout << "\tCoordinates: int\n";
  else if(*types_[attribute_num_] == typeid(int64_t))
    std::cout << "\tCoordinates: int64_t\n";
  else if(*types_[attribute_num_] == typeid(float))
    std::cout << "\tCoordinates: float\n";
  else if(*types_[attribute_num_] == typeid(double))
    std::cout << "\tCoordinates: double\n";

  std::cout << "Cell sizes:\n";
  for(int i=0; i<=attribute_num_; ++i) {
    std::cout << "\t" << ((i==attribute_num_) ? "Coordinates: "  
                                              : attribute_names_[i]) 
                      << ": ";
    if(cell_sizes_[i] == VAR_SIZE)
      std::cout << "var\n"; 
    else
      std::cout << cell_sizes_[i] << "\n"; 
  }

  std::cout << "Compression types:\n";
  for(int i=0; i<attribute_num_; ++i)
    if(compression_[i] == RLE)
      std::cout << "\t" << attribute_names_[i] << ": RLE\n";
    else if(compression_[i] == ZIP)
      std::cout << "\t" << attribute_names_[i] << ": ZIP\n";
    else if(compression_[i] == LZ)
      std::cout << "\t" << attribute_names_[i] << ": LZ\n";
    else if(compression_[i] == NONE)
      std::cout << "\t" << attribute_names_[i] << ": NONE\n";
  if(compression_[attribute_num_] == RLE)
    std::cout << "\tCoordinates: RLE\n";
  else if(compression_[attribute_num_] == ZIP)
    std::cout << "\tCoordinates: ZIP\n";
  else if(compression_[attribute_num_] == LZ)
    std::cout << "\tCoordinates: LZ\n";
  else if(compression_[attribute_num_] == NONE)
    std::cout << "\tCoordinates: NONE\n";
}

bool ArraySchema::succeeds(const void* coords_A,
                           const void* coords_B) const {
  if(*types_[attribute_num_] == typeid(int)) {
    return succeeds(static_cast<const int*>(coords_A), 
                    static_cast<const int*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(int64_t)) {
    return succeeds(static_cast<const int64_t*>(coords_A), 
                    static_cast<const int64_t*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(float)) {
    return succeeds(static_cast<const float*>(coords_A), 
                    static_cast<const float*>(coords_B));
  } else if(*types_[attribute_num_] == typeid(double)) {
    return succeeds(static_cast<const double*>(coords_A), 
                    static_cast<const double*>(coords_B));
  } else {
    assert(0); // The code must never reach this point
  }
}

template<class T>
bool ArraySchema::succeeds(const T* coords_A,
                           const T* coords_B) const {
  assert(*types_[attribute_num_] == typeid(T));

  // ROW_MAJOR order
  if(cell_order_ == CO_ROW_MAJOR) {
    for(int i=0; i<dim_num_; ++i) {
      if(coords_A[i] > coords_B[i])
        return true;
      else if(coords_A[i] < coords_B[i])
        return false;
      // else coordinate is equal - check the next dimension
    }
    return false; // Coordinates are equal
  // COLUMN_MAJOR order
  } else if(cell_order_ == CO_COLUMN_MAJOR) {
    for(int i=dim_num_-1; i>=0; --i) {
      if(coords_A[i] > coords_B[i])
        return true;
      else if(coords_A[i] < coords_B[i])
        return false;
      // else coordinate is equal - check the next dimension
    }
    return false; // Coordinates are equal
  // HILBERT order
  } else if(cell_order_ == CO_HILBERT) {
    int64_t cell_id_A = cell_id_hilbert(coords_A);
    int64_t cell_id_B = cell_id_hilbert(coords_B);
    if(cell_id_A > cell_id_B) {
      return true;
    } else if (cell_id_A < cell_id_B) {
      return false;
    } else { // (cell_id_A == cell_id_B): check ROW_MAJOR order
      for(int i=0; i<dim_num_; ++i) {
        if(coords_A[i] > coords_B[i])
          return true;
        else if(coords_A[i] < coords_B[i])
          return false;
        // else coordinate is equal - check the next dimension
      }
      return false; // Coordinates are equal
    }
  } else { // it should never reach this point
    assert(0);
  }
}

int64_t ArraySchema::tile_id_column_major(const void* coords) const {
  if(*(types_[attribute_num_]) == typeid(int))
    return tile_id_column_major(static_cast<const int*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(int64_t))
    return tile_id_column_major(static_cast<const int64_t*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(float))
    return tile_id_column_major(static_cast<const float*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(double))
    return tile_id_column_major(static_cast<const double*>(coords));  
}

template<typename T>
int64_t ArraySchema::tile_id_column_major(const T* coords) const {
  assert(check_on_tile_id_request(coords));
 
  int64_t tile_ID = 0;
  int64_t partition_id;
  for(int i = 0; i < dim_num_; ++i) {
    partition_id = floor(coords[i] / tile_extents_[i]);
    tile_ID += partition_id * tile_id_offsets_column_major_[i];
  }	

  return tile_ID;
}

int64_t ArraySchema::tile_id_hilbert(const void* coords) const {
  if(*(types_[attribute_num_]) == typeid(int))
    return tile_id_hilbert(static_cast<const int*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(int64_t))
    return tile_id_hilbert(static_cast<const int64_t*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(float))
    return tile_id_hilbert(static_cast<const float*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(double))
    return tile_id_hilbert(static_cast<const double*>(coords));  
}

template<typename T>
int64_t ArraySchema::tile_id_hilbert(const T* coords) const {
  assert(check_on_tile_id_request(coords));
  	
  HilbertCurve *hc = new HilbertCurve();
  int *int_coords = new int[dim_num_];

  for(int i = 0; i < dim_num_; ++i) 
    int_coords[i] = static_cast<int>(coords[i]/tile_extents_[i]);

  int64_t tile_ID = hc->AxestoLine(int_coords, hilbert_tile_bits_, dim_num_);	

  delete hc;
  delete [] int_coords;

  return tile_ID;
}

int64_t ArraySchema::tile_id_row_major(const void* coords) const {
  if(*(types_[attribute_num_]) == typeid(int))
    return tile_id_row_major(static_cast<const int*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(int64_t))
    return tile_id_row_major(static_cast<const int64_t*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(float))
    return tile_id_row_major(static_cast<const float*>(coords));  
  else if(*(types_[attribute_num_]) == typeid(double))
    return tile_id_row_major(static_cast<const double*>(coords));  
}

template<typename T>
int64_t ArraySchema::tile_id_row_major(const T* coords) const {
  check_on_tile_id_request(coords);
 
  int64_t tile_ID = 0;
  int64_t partition_id;
  for(int i = 0; i < dim_num_; ++i) {
    partition_id = floor(coords[i] / tile_extents_[i]);
    tile_ID += partition_id * tile_id_offsets_row_major_[i];
  }	

  return tile_ID;
}

const ArraySchema* ArraySchema::transpose(
    const std::string& new_array_name) const {
  assert(dim_num_ == 2);

  // Copy array schema and give new name
  ArraySchema* new_array_schema = new ArraySchema();
  *new_array_schema = *this;
  new_array_schema->array_name_ = new_array_name;
  
  // Transpose the dim domains
  std::vector<std::pair<double, double> > new_dim_domains;
  new_dim_domains.push_back(dim_domains_[1]);
  new_dim_domains.push_back(dim_domains_[0]);
  new_array_schema->dim_domains_ = new_dim_domains;

  return new_array_schema;
}

/******************************************************
******************* PRIVATE METHODS *******************
******************************************************/

template<typename T>
bool ArraySchema::check_on_tile_id_request(const T* coords) const {
  if(has_irregular_tiles() || *types_[attribute_num_] != typeid(T))
    return false; 

  for(int i=0; i<dim_num_; ++i) 
    if(coords[i] < dim_domains_[i].first ||
       coords[i] > dim_domains_[i].second)
      return false;

  return true;
}

size_t ArraySchema::compute_cell_size(int i) const {
  assert(i>= 0 && i <= attribute_num_);

  // Variable-sized cell
  if(i<attribute_num_ && val_num_[i] == VAR_SIZE)
    return VAR_SIZE;

  // Fixed-sized cell
  size_t size;
  
  // Attributes
  if(i < attribute_num_) {
    if(types_[i] == &typeid(char))
      size = val_num_[i] * sizeof(char);
    else if(types_[i] == &typeid(int))
      size = val_num_[i] * sizeof(int);
    else if(types_[i] == &typeid(int64_t))
      size = val_num_[i] * sizeof(int64_t);
    else if(types_[i] == &typeid(float))
      size = val_num_[i] * sizeof(float);
    else if(types_[i] == &typeid(double))
      size = val_num_[i] * sizeof(double);
  } else { // Coordinates
    if(types_[i] == &typeid(int))
      size = dim_num_ * sizeof(int);
    else if(types_[i] == &typeid(int64_t))
      size = dim_num_ * sizeof(int64_t);
    else if(types_[i] == &typeid(float))
      size = dim_num_ * sizeof(float);
    else if(types_[i] == &typeid(double))
      size = dim_num_ * sizeof(double);
  }

  return size; 
}

void ArraySchema::compute_hilbert_cell_bits() {
  double max_domain_range = 0;
  double domain_range;
  bool regular = (tile_extents_.size() != 0);

  for(int i = 0; i < dim_num_; ++i) { 
    if(regular) // Regular tiles: ids are calculated within a tile
      domain_range = tile_extents_[i];
    else        // Irregular tiles: ids are calculated in the entire domain
      domain_range = dim_domains_[i].second - dim_domains_[i].first + 1;
    if(max_domain_range < domain_range)
      max_domain_range = domain_range;
  }

  hilbert_cell_bits_ = ceil(log2(static_cast<int64_t>(max_domain_range+0.5)));
}

void ArraySchema::compute_hilbert_tile_bits() {
  assert(has_regular_tiles());

  double max_domain_range = 0;
  double domain_range;

  for(int i = 0; i < dim_num_; ++i) {       
    domain_range = (dim_domains_[i].second - dim_domains_[i].first + 1) /
                    tile_extents_[i];  
    if(max_domain_range < domain_range)
      max_domain_range = domain_range;
  }

  hilbert_tile_bits_ = ceil(log2(static_cast<int64_t>(max_domain_range+0.5)));
}

void ArraySchema::compute_tile_id_offsets() {
  assert(has_regular_tiles());
  
  double domain_range;
  int64_t partition_num; // Number of partitions on some axis
  int64_t offset_row = 1;
  int64_t offset_column = 1;

  tile_id_offsets_row_major_.push_back(offset_row);	
  tile_id_offsets_column_major_.push_back(offset_column);	

  for(int i=0; i<dim_num_-1 ; ++i) {
    // Row major
    domain_range = dim_domains_[i].second - dim_domains_[i].first + 1;
    partition_num = ceil(domain_range / tile_extents_[i]);
    offset_row *= partition_num;
    tile_id_offsets_row_major_.push_back(offset_row);
   
    // Column major
    domain_range = dim_domains_[dim_num_-1-i].second - 
                   dim_domains_[dim_num_-1-i].first + 1;
    partition_num = ceil(domain_range / tile_extents_[dim_num_-1-i]);
    offset_column *= partition_num;
    tile_id_offsets_column_major_.push_back(offset_column);
  }
 
  // For column major only 
  std::reverse(tile_id_offsets_column_major_.begin(), 
               tile_id_offsets_column_major_.end());
}

size_t ArraySchema::compute_type_size(int i) const {
  assert(i>= 0 && i <= attribute_num_);

  if(types_[i] == &typeid(char))
    return sizeof(char);
  else if(types_[i] == &typeid(int))
    return sizeof(int);
  else if(types_[i] == &typeid(int64_t))
    return sizeof(int64_t);
  else if(types_[i] == &typeid(float))
    return sizeof(float);
  else if(types_[i] == &typeid(double))
    return sizeof(double);
}

std::pair<ArraySchema::AttributeIds, ArraySchema::AttributeIds>
ArraySchema::get_attribute_ids(
    const std::set<std::string>& attribute_names) const {
  // Get the ids of the attribute names corresponding to the input names
  std::vector<int> attribute_ids;
  std::set<std::string>::const_iterator attr_it = 
      attribute_names.begin();
  std::set<std::string>::const_iterator attr_it_end = 
      attribute_names.end();
  for(; attr_it != attr_it_end; ++attr_it) 
    attribute_ids.push_back(attribute_id(*attr_it));
  std::sort(attribute_ids.begin(), attribute_ids.end());
  int input_attribute_num = attribute_ids.size();

  // Find the ids of the attributes NOT corresponding to the input names
  std::vector<int> non_attribute_ids;
  for(int j=0; j<attribute_ids[0]; ++j)
    non_attribute_ids.push_back(j);
  for(int i=1; i<input_attribute_num; ++i) {
    for(int j=attribute_ids[i-1]+1; j<attribute_ids[i]; ++j)
      non_attribute_ids.push_back(j);
  }
  for(int j=attribute_ids[input_attribute_num-1] + 1; 
      j<=attribute_num_; ++j)
    non_attribute_ids.push_back(j);

  return std::pair<AttributeIds, AttributeIds>(attribute_ids, 
                                               non_attribute_ids);
}

bool ArraySchema::valid_attribute_ids(
    const std::vector<int>& attribute_ids) const {
  for(int i=0; i<attribute_ids.size(); ++i) {
    if(attribute_ids[i] < 0 || attribute_ids[i] > attribute_num_) 
      return false;
  }

  return true;
}

// Explicit template instantiations
template bool ArraySchema::precedes<int>(
    const int* coords_A, const int* coords_B) const;
template bool ArraySchema::precedes<int64_t>(
    const int64_t* coords_A, const int64_t* coords_B) const;
template bool ArraySchema::precedes<float>(
    const float* coords_A, const float* coords_B) const;
template bool ArraySchema::precedes<double>(
    const double* coords_A, const double* coords_B) const;
template bool ArraySchema::succeeds<int>(
    const int* coords_A, const int* coords_B) const;
template bool ArraySchema::succeeds<int64_t>(
    const int64_t* coords_A, const int64_t* coords_B) const;
template bool ArraySchema::succeeds<float>(
    const float* coords_A, const float* coords_B) const;
template bool ArraySchema::succeeds<double>(
    const double* coords_A, const double* coords_B) const;
template int64_t ArraySchema::cell_id_hilbert<int>(
    const int* coords) const;
template int64_t ArraySchema::cell_id_hilbert<int64_t>(
    const int64_t* coords) const;
template int64_t ArraySchema::cell_id_hilbert<float>(
    const float* coords) const;
template int64_t ArraySchema::cell_id_hilbert<double>(
    const double* coords) const;
template int64_t ArraySchema::tile_id_row_major<int>(
    const int* coords) const;
template int64_t ArraySchema::tile_id_row_major<int64_t>(
    const int64_t* coords) const;
template int64_t ArraySchema::tile_id_row_major<float>(
    const float* coords) const;
template int64_t ArraySchema::tile_id_row_major<double>(
    const double* coords) const;
template int64_t ArraySchema::tile_id_column_major<int>(
    const int* coords) const;
template int64_t ArraySchema::tile_id_column_major<int64_t>(
    const int64_t* coords) const;
template int64_t ArraySchema::tile_id_column_major<float>(
    const float* coords) const;
template int64_t ArraySchema::tile_id_column_major<double>(
    const double* coords) const;
template int64_t ArraySchema::tile_id_hilbert<int>(
    const int* coordinates) const;
template int64_t ArraySchema::tile_id_hilbert<int64_t>(
    const int64_t* coords) const;
template int64_t ArraySchema::tile_id_hilbert<float>(
    const float* coords) const;
template int64_t ArraySchema::tile_id_hilbert<double>(
    const double* coords) const;