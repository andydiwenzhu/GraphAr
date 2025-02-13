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

#include "../config.h"
#include "gar/graph.h"
#include "gar/graph_info.h"
#include "gar/reader/arrow_chunk_reader.h"
#include "gar/writer/arrow_chunk_writer.h"
#include "gar/writer/edges_builder.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("test_bfs_with_father_example") {
  // read file and construct graph info
  std::string path =
      TEST_DATA_DIR + "/ldbc_sample/parquet/ldbc_sample.graph.yml";
  auto graph_info = GAR_NAMESPACE::GraphInfo::Load(path).value();

  // get the person vertices of graph
  std::string label = "person";
  REQUIRE(graph_info.GetVertexInfo(label).status().ok());
  auto maybe_vertices =
      GAR_NAMESPACE::ConstructVerticesCollection(graph_info, label);
  REQUIRE(maybe_vertices.status().ok());
  auto& vertices = maybe_vertices.value();
  int num_vertices = vertices.size();
  std::cout << "num_vertices: " << num_vertices << std::endl;

  // get the "person_knows_person" edges of graph
  std::string src_label = "person", edge_label = "knows", dst_label = "person";
  auto maybe_edges = GAR_NAMESPACE::ConstructEdgesCollection(
      graph_info, src_label, edge_label, dst_label,
      GAR_NAMESPACE::AdjListType::unordered_by_source);
  REQUIRE(!maybe_edges.has_error());
  auto& edges = std::get<GAR_NAMESPACE::EdgesCollection<
      GAR_NAMESPACE::AdjListType::unordered_by_source>>(maybe_edges.value());

  // run bfs algorithm
  GAR_NAMESPACE::IdType root = 0;
  std::vector<int32_t> distance(num_vertices);
  std::vector<GAR_NAMESPACE::IdType> pre(num_vertices);
  for (GAR_NAMESPACE::IdType i = 0; i < num_vertices; i++) {
    distance[i] = (i == root ? 0 : -1);
    pre[i] = (i == root ? root : -1);
  }
  auto it_begin = edges.begin(), it_end = edges.end();
  for (int iter = 0;; iter++) {
    GAR_NAMESPACE::IdType count = 0;
    for (auto it = it_begin; it != it_end; ++it) {
      auto src = it.source(), dst = it.destination();
      if (distance[src] == iter && distance[dst] == -1) {
        distance[dst] = distance[src] + 1;
        pre[dst] = src;
        count++;
      }
    }
    std::cout << "iter " << iter << ": " << count << " vertices." << std::endl;
    if (count == 0)
      break;
  }
  for (int i = 0; i < num_vertices; i++) {
    std::cout << i << ", distance: " << distance[i] << ", father: " << pre[i]
              << std::endl;
  }

  // Append the bfs result to the vertex info as a property group
  // and write to file
  // construct property group
  GAR_NAMESPACE::Property bfs = {
      "bfs", GAR_NAMESPACE::DataType(GAR_NAMESPACE::Type::INT32), false};
  GAR_NAMESPACE::Property father = {
      "father", GAR_NAMESPACE::DataType(GAR_NAMESPACE::Type::INT64), false};
  std::vector<GAR_NAMESPACE::Property> property_vector = {bfs, father};
  GAR_NAMESPACE::PropertyGroup group(property_vector,
                                     GAR_NAMESPACE::FileType::CSV);

  // extend the vertex_info
  auto maybe_vertex_info = graph_info.GetVertexInfo(label);
  REQUIRE(maybe_vertex_info.status().ok());
  auto vertex_info = maybe_vertex_info.value();
  auto maybe_extend_info = vertex_info.Extend(group);
  REQUIRE(maybe_extend_info.status().ok());
  auto extend_info = maybe_extend_info.value();

  // dump the extened vertex info
  REQUIRE(extend_info.IsValidated());
  REQUIRE(extend_info.Dump().status().ok());
  REQUIRE(extend_info.Save("/tmp/person-new-bfs-father.vertex.yml").ok());
  // construct vertex property writer
  GAR_NAMESPACE::VertexPropertyWriter writer(extend_info, "file:///tmp/");
  // convert results to arrow::Table
  std::vector<std::shared_ptr<arrow::Array>> arrays;
  std::vector<std::shared_ptr<arrow::Field>> schema_vector;
  schema_vector.push_back(arrow::field(
      bfs.name, GAR_NAMESPACE::DataType::DataTypeToArrowDataType(bfs.type)));
  schema_vector.push_back(arrow::field(
      father.name,
      GAR_NAMESPACE::DataType::DataTypeToArrowDataType(father.type)));
  arrow::Int32Builder array_builder1;
  REQUIRE(array_builder1.Reserve(num_vertices).ok());
  REQUIRE(array_builder1.AppendValues(distance).ok());
  std::shared_ptr<arrow::Array> array1 = array_builder1.Finish().ValueOrDie();
  arrays.push_back(array1);

  arrow::Int64Builder array_builder2;
  REQUIRE(array_builder2.Reserve(num_vertices).ok());
  for (int i = 0; i < num_vertices; i++) {
    if (pre[i] == -1) {
      REQUIRE(array_builder2.AppendNull().ok());
    } else {
      auto it = vertices.find(pre[i]);
      auto father_id = it.property<int64_t>("id").value();
      REQUIRE(array_builder2.Append(father_id).ok());
    }
  }
  std::shared_ptr<arrow::Array> array2 = array_builder2.Finish().ValueOrDie();
  arrays.push_back(array2);

  auto schema = std::make_shared<arrow::Schema>(schema_vector);
  std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, arrays);
  // dump the results through writer
  REQUIRE(writer.WriteTable(table, group, 0).ok());

  // construct a new graph
  src_label = "person";
  edge_label = "bfs";
  dst_label = "person";
  int edge_chunk_size = 1024, src_chunk_size = 100, dst_chunk_size = 100;
  bool directed = true;
  GAR_NAMESPACE::InfoVersion version(1);
  GAR_NAMESPACE::EdgeInfo new_edge_info(src_label, edge_label, dst_label,
                                        edge_chunk_size, src_chunk_size,
                                        dst_chunk_size, directed, version);
  REQUIRE(new_edge_info
              .AddAdjList(GAR_NAMESPACE::AdjListType::ordered_by_source,
                          GAR_NAMESPACE::FileType::CSV)
              .ok());
  REQUIRE(new_edge_info.IsValidated());
  // save & dump
  REQUIRE(!new_edge_info.Dump().has_error());
  REQUIRE(new_edge_info.Save("/tmp/person_bfs_person.edge.yml").ok());
  GAR_NAMESPACE::builder::EdgesBuilder edges_builder(
      new_edge_info, "file:///tmp/",
      GAR_NAMESPACE::AdjListType::ordered_by_source);
  for (int i = 0; i < num_vertices; i++) {
    if (i == root || pre[i] == -1)
      continue;
    GAR_NAMESPACE::builder::Edge e(pre[i], i);
    REQUIRE(edges_builder.AddEdge(e).ok());
  }
  REQUIRE(edges_builder.Dump().ok());
}
