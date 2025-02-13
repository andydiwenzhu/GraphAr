/** Copyright 2022 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <iostream>

#include "arrow/api.h"

#include "gar/reader/arrow_chunk_reader.h"
#include "gar/utils/reader_utils.h"

namespace GAR_NAMESPACE_INTERNAL {

Result<std::shared_ptr<arrow::Table>>
VertexPropertyArrowChunkReader::GetChunk() noexcept {
  if (chunk_table_ == nullptr) {
    GAR_ASSIGN_OR_RAISE(
        auto chunk_file_path,
        vertex_info_.GetFilePath(property_group_, chunk_index_));
    std::string path = prefix_ + chunk_file_path;
    GAR_ASSIGN_OR_RAISE(chunk_table_, fs_->ReadFileToTable(
                                          path, property_group_.GetFileType()));
  }
  IdType row_offset = seek_id_ - chunk_index_ * vertex_info_.GetChunkSize();
  return chunk_table_->Slice(row_offset);
}

Result<std::pair<IdType, IdType>>
VertexPropertyArrowChunkReader::GetRange() noexcept {
  if (chunk_table_ == nullptr) {
    return Status::InvalidOperation("The GetRange operation is not invalid.");
  }
  IdType row_offset = seek_id_ - chunk_index_ * vertex_info_.GetChunkSize();
  return std::make_pair(seek_id_,
                        seek_id_ + chunk_table_->num_rows() - row_offset);
}

Status AdjListArrowChunkReader::seek_src(IdType id) noexcept {
  if (adj_list_type_ != AdjListType::unordered_by_source &&
      adj_list_type_ != AdjListType::ordered_by_source) {
    return Status::InvalidOperation(
        "The seek_src operation is invalid in reader.");
  }

  IdType new_vertex_chunk_index = id / edge_info_.GetSrcChunkSize();
  if (new_vertex_chunk_index >= vertex_chunk_num_) {
    return Status::KeyError("The id " + std::to_string(id) + " not exist.");
  }
  if (vertex_chunk_index_ != new_vertex_chunk_index) {
    vertex_chunk_index_ = new_vertex_chunk_index;
    std::string chunk_dir =
        base_dir_ + "/part" + std::to_string(vertex_chunk_index_);
    GAR_ASSIGN_OR_RAISE(chunk_num_, fs_->GetFileNumOfDir(chunk_dir));
    chunk_table_.reset();
  }

  if (adj_list_type_ == AdjListType::unordered_by_source) {
    return seek(0);  // start from first chunk
  } else {
    GAR_ASSIGN_OR_RAISE(auto range,
                        utils::GetAdjListOffsetOfVertex(edge_info_, prefix_,
                                                        adj_list_type_, id));
    return seek(range.first);
  }
  return Status::OK();
}

Status AdjListArrowChunkReader::seek_dst(IdType id) noexcept {
  if (adj_list_type_ != AdjListType::unordered_by_dest &&
      adj_list_type_ != AdjListType::ordered_by_dest) {
    return Status::InvalidOperation(
        "The seek_dst operation is invalid in reader.");
  }

  IdType new_vertex_chunk_index = id / edge_info_.GetDstChunkSize();
  if (new_vertex_chunk_index >= vertex_chunk_num_) {
    return Status::KeyError("The id " + std::to_string(id) + " not exist.");
  }
  if (vertex_chunk_index_ != new_vertex_chunk_index) {
    vertex_chunk_index_ = new_vertex_chunk_index;
    std::string chunk_dir =
        base_dir_ + "/part" + std::to_string(vertex_chunk_index_);
    GAR_ASSIGN_OR_RAISE(chunk_num_, fs_->GetFileNumOfDir(chunk_dir));
    chunk_table_.reset();
  }

  if (adj_list_type_ == AdjListType::unordered_by_dest) {
    return seek(0);  // start from the first chunk
  } else {
    GAR_ASSIGN_OR_RAISE(auto range,
                        utils::GetAdjListOffsetOfVertex(edge_info_, prefix_,
                                                        adj_list_type_, id));
    return seek(range.first);
  }
}

Result<std::shared_ptr<arrow::Table>>
AdjListArrowChunkReader::GetChunk() noexcept {
  if (chunk_table_ == nullptr) {
    GAR_ASSIGN_OR_RAISE(auto chunk_file_path,
                        edge_info_.GetAdjListFilePath(
                            vertex_chunk_index_, chunk_index_, adj_list_type_));
    std::string path = prefix_ + chunk_file_path;
    GAR_ASSIGN_OR_RAISE(auto file_type,
                        edge_info_.GetAdjListFileType(adj_list_type_));
    GAR_ASSIGN_OR_RAISE(chunk_table_, fs_->ReadFileToTable(path, file_type));
  }
  IdType row_offset = seek_offset_ - chunk_index_ * edge_info_.GetChunkSize();
  return chunk_table_->Slice(row_offset);
}

Result<IdType> AdjListArrowChunkReader::GetRowNumOfChunk() noexcept {
  if (chunk_table_ == nullptr) {
    GAR_ASSIGN_OR_RAISE(auto chunk_file_path,
                        edge_info_.GetAdjListFilePath(
                            vertex_chunk_index_, chunk_index_, adj_list_type_));
    std::string path = prefix_ + chunk_file_path;
    GAR_ASSIGN_OR_RAISE(auto file_type,
                        edge_info_.GetAdjListFileType(adj_list_type_));
    GAR_ASSIGN_OR_RAISE(chunk_table_, fs_->ReadFileToTable(path, file_type));
  }
  return chunk_table_->num_rows();
}

Status AdjListPropertyArrowChunkReader::seek_src(IdType id) noexcept {
  if (adj_list_type_ != AdjListType::unordered_by_source &&
      adj_list_type_ != AdjListType::ordered_by_source) {
    return Status::InvalidOperation(
        "The seek_src operation is invalid in reader.");
  }

  IdType new_vertex_chunk_index = id / edge_info_.GetSrcChunkSize();
  if (new_vertex_chunk_index >= vertex_chunk_num_) {
    return Status::KeyError("The id " + std::to_string(id) + " not exist.");
  }
  if (vertex_chunk_index_ != new_vertex_chunk_index) {
    vertex_chunk_index_ = new_vertex_chunk_index;
    std::string chunk_dir =
        base_dir_ + "/part" + std::to_string(vertex_chunk_index_);
    GAR_ASSIGN_OR_RAISE(chunk_num_, fs_->GetFileNumOfDir(chunk_dir));
    chunk_table_.reset();
  }

  if (adj_list_type_ == AdjListType::unordered_by_source) {
    return seek(0);  // start from first chunk
  } else {
    GAR_ASSIGN_OR_RAISE(auto range,
                        utils::GetAdjListOffsetOfVertex(edge_info_, prefix_,
                                                        adj_list_type_, id));
    return seek(range.first);
  }
  return Status::OK();
}

Status AdjListPropertyArrowChunkReader::seek_dst(IdType id) noexcept {
  if (adj_list_type_ != AdjListType::unordered_by_dest &&
      adj_list_type_ != AdjListType::ordered_by_dest) {
    return Status::InvalidOperation(
        "The seek_dst operation is invalid in reader.");
  }

  IdType new_vertex_chunk_index = id / edge_info_.GetDstChunkSize();
  if (new_vertex_chunk_index >= vertex_chunk_num_) {
    return Status::KeyError("The id " + std::to_string(id) + " not exist.");
  }
  if (vertex_chunk_index_ != new_vertex_chunk_index) {
    vertex_chunk_index_ = new_vertex_chunk_index;
    std::string chunk_dir =
        base_dir_ + "/part" + std::to_string(vertex_chunk_index_);
    GAR_ASSIGN_OR_RAISE(chunk_num_, fs_->GetFileNumOfDir(chunk_dir));
    chunk_table_.reset();
  }

  if (adj_list_type_ == AdjListType::unordered_by_dest) {
    return seek(0);  // start from the first chunk
  } else {
    GAR_ASSIGN_OR_RAISE(auto range,
                        utils::GetAdjListOffsetOfVertex(edge_info_, prefix_,
                                                        adj_list_type_, id));
    return seek(range.first);
  }
}

Result<std::shared_ptr<arrow::Array>>
AdjListOffsetArrowChunkReader::GetChunk() noexcept {
  if (chunk_table_ == nullptr) {
    GAR_ASSIGN_OR_RAISE(
        auto chunk_file_path,
        edge_info_.GetAdjListOffsetFilePath(chunk_index_, adj_list_type_));
    std::string path = prefix_ + chunk_file_path;
    GAR_ASSIGN_OR_RAISE(auto file_type,
                        edge_info_.GetAdjListFileType(adj_list_type_));
    GAR_ASSIGN_OR_RAISE(chunk_table_, fs_->ReadFileToTable(path, file_type));
  }
  IdType row_offset = seek_id_ - chunk_index_ * vertex_chunk_size_;
  return chunk_table_->Slice(row_offset)->column(0)->chunk(0);
}

Result<std::shared_ptr<arrow::Table>>
AdjListPropertyArrowChunkReader::GetChunk() noexcept {
  if (chunk_table_ == nullptr) {
    GAR_ASSIGN_OR_RAISE(
        auto chunk_file_path,
        edge_info_.GetPropertyFilePath(property_group_, adj_list_type_,
                                       vertex_chunk_index_, chunk_index_));
    std::string path = prefix_ + chunk_file_path;
    GAR_ASSIGN_OR_RAISE(chunk_table_, fs_->ReadFileToTable(
                                          path, property_group_.GetFileType()));
  }
  IdType row_offset = seek_offset_ - chunk_index_ * edge_info_.GetChunkSize();
  return chunk_table_->Slice(row_offset);
}

}  // namespace GAR_NAMESPACE_INTERNAL
