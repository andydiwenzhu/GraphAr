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

#include <cstdlib>

#include "./config.h"
#include "gar/reader/chunk_info_reader.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("test_vertex_property_chunk_info_reader") {
  // read file and construct graph info
  std::string path =
      TEST_DATA_DIR + "/ldbc_sample/parquet/ldbc_sample.graph.yml";
  auto maybe_graph_info = GAR_NAMESPACE::GraphInfo::Load(path);
  REQUIRE(maybe_graph_info.status().ok());
  auto graph_info = maybe_graph_info.value();
  REQUIRE(graph_info.GetAllVertexInfo().size() == 1);
  REQUIRE(graph_info.GetAllEdgeInfo().size() == 1);

  // construct vertex property info reader
  std::string label = "person", property_name = "id";
  REQUIRE(graph_info.GetVertexInfo(label).status().ok());
  auto maybe_group = graph_info.GetVertexPropertyGroup(label, property_name);
  REQUIRE(!maybe_group.has_error());
  const GAR_NAMESPACE::PropertyGroup& group = maybe_group.value();
  auto maybe_reader = GAR_NAMESPACE::ConstructVertexPropertyChunkInfoReader(
      graph_info, label, group);
  REQUIRE(!maybe_reader.has_error());
  GAR_NAMESPACE::VertexPropertyChunkInfoReader& reader = maybe_reader.value();

  // get chunk file path & validate
  auto maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  std::string chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path ==
          TEST_DATA_DIR + "/ldbc_sample/parquet/vertex/person/id/part0/chunk0");
  REQUIRE(reader.seek(520).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path ==
          TEST_DATA_DIR + "/ldbc_sample/parquet/vertex/person/id/part5/chunk0");
  REQUIRE(reader.next_chunk().ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path ==
          TEST_DATA_DIR + "/ldbc_sample/parquet/vertex/person/id/part6/chunk0");
  REQUIRE(reader.seek(900).ok());
  maybe_chunk_path = reader.GetChunk();
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path ==
          TEST_DATA_DIR + "/ldbc_sample/parquet/vertex/person/id/part9/chunk0");
  // now is end of the chunks
  REQUIRE(reader.next_chunk().IsOutOfRange());

  // test seek the id not in the chunks
  REQUIRE(reader.seek(100000).IsKeyError());

  // test Get vertex property chunk num through vertex property chunk info
  // reader
  REQUIRE(reader.GetChunkNum() == 10);
}

TEST_CASE("test_adj_list_chunk_info_reader") {
  // read file and construct graph info
  std::string path =
      TEST_DATA_DIR + "/ldbc_sample/parquet/ldbc_sample.graph.yml";
  auto maybe_graph_info = GAR_NAMESPACE::GraphInfo::Load(path);
  REQUIRE(maybe_graph_info.status().ok());
  auto graph_info = maybe_graph_info.value();
  REQUIRE(graph_info.GetAllVertexInfo().size() == 1);
  REQUIRE(graph_info.GetAllEdgeInfo().size() == 1);

  // construct adj list info reader
  std::string src_label = "person", edge_label = "knows", dst_label = "person";
  auto maybe_reader = GAR_NAMESPACE::ConstructAdjListChunkInfoReader(
      graph_info, src_label, edge_label, dst_label,
      GAR_NAMESPACE::AdjListType::ordered_by_source);
  REQUIRE(maybe_reader.status().ok());
  auto& reader = maybe_reader.value();

  // get chunk file path & validate
  auto maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  auto chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/adj_list/part0/chunk0");
  REQUIRE(reader.seek(100).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/adj_list/part0/chunk0");
  REQUIRE(reader.next_chunk().ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/adj_list/part1/chunk0");

  // seek_src & seek_dst
  REQUIRE(reader.seek_src(100).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/adj_list/part1/chunk0");
  REQUIRE(reader.seek_src(900).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/adj_list/part9/chunk0");
  REQUIRE(reader.next_chunk().IsOutOfRange());

  // seek an invalid src id
  REQUIRE(reader.seek_src(1000).IsKeyError());
  REQUIRE(reader.seek_dst(100).IsInvalidOperation());

  // test reader to read ordered by dest
  auto maybe_dst_reader = GAR_NAMESPACE::ConstructAdjListChunkInfoReader(
      graph_info, src_label, edge_label, dst_label,
      GAR_NAMESPACE::AdjListType::ordered_by_dest);
  REQUIRE(maybe_dst_reader.status().ok());
  auto& dst_reader = maybe_dst_reader.value();
  REQUIRE(dst_reader.seek_dst(100).ok());
  maybe_chunk_path = dst_reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_dest/adj_list/part1/chunk0");

  // seek an invalid dst id
  REQUIRE(dst_reader.seek_dst(1000).IsKeyError());
  REQUIRE(dst_reader.seek_src(100).IsInvalidOperation());
}

TEST_CASE("test_adj_list_property_chunk_info_reader") {
  // read file and construct graph info
  std::string path =
      TEST_DATA_DIR + "/ldbc_sample/parquet/ldbc_sample.graph.yml";
  auto maybe_graph_info = GAR_NAMESPACE::GraphInfo::Load(path);
  REQUIRE(maybe_graph_info.status().ok());
  auto graph_info = maybe_graph_info.value();

  std::string src_label = "person", edge_label = "knows", dst_label = "person",
              property_name = "creationDate";

  auto maybe_group = graph_info.GetEdgePropertyGroup(
      src_label, edge_label, dst_label, property_name,
      GAR_NAMESPACE::AdjListType::ordered_by_source);
  REQUIRE(maybe_group.status().ok());
  auto group = maybe_group.value();
  auto maybe_property_reader =
      GAR_NAMESPACE::ConstructAdjListPropertyChunkInfoReader(
          graph_info, src_label, edge_label, dst_label, group,
          GAR_NAMESPACE::AdjListType::ordered_by_source);
  REQUIRE(maybe_property_reader.status().ok());
  auto reader = maybe_property_reader.value();

  // get chunk file path & validate
  auto maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  auto chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/creationDate/part0/chunk0");
  REQUIRE(reader.seek(100).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/creationDate/part0/chunk0");
  REQUIRE(reader.next_chunk().ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/creationDate/part1/chunk0");

  // seek_src & seek_dst
  REQUIRE(reader.seek_src(100).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/creationDate/part1/chunk0");
  REQUIRE(reader.seek_src(900).ok());
  maybe_chunk_path = reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_source/creationDate/part9/chunk0");
  REQUIRE(reader.next_chunk().IsOutOfRange());

  // seek an invalid src id
  REQUIRE(reader.seek_src(1000).IsKeyError());
  REQUIRE(reader.seek_dst(100).IsInvalidOperation());

  // test reader to read ordered by dest
  maybe_group = graph_info.GetEdgePropertyGroup(
      src_label, edge_label, dst_label, property_name,
      GAR_NAMESPACE::AdjListType::ordered_by_dest);
  REQUIRE(maybe_group.status().ok());
  group = maybe_group.value();
  auto maybe_dst_reader =
      GAR_NAMESPACE::ConstructAdjListPropertyChunkInfoReader(
          graph_info, src_label, edge_label, dst_label, group,
          GAR_NAMESPACE::AdjListType::ordered_by_dest);
  REQUIRE(maybe_dst_reader.status().ok());
  auto& dst_reader = maybe_dst_reader.value();
  REQUIRE(dst_reader.seek_dst(100).ok());
  maybe_chunk_path = dst_reader.GetChunk();
  REQUIRE(maybe_chunk_path.status().ok());
  chunk_path = maybe_chunk_path.value();
  REQUIRE(chunk_path == TEST_DATA_DIR +
                            "/ldbc_sample/parquet/edge/person_knows_person/"
                            "ordered_by_dest/creationDate/part1/chunk0");

  // seek an invalid dst id
  REQUIRE(dst_reader.seek_dst(1000).IsKeyError());
  REQUIRE(dst_reader.seek_src(100).IsInvalidOperation());
}
