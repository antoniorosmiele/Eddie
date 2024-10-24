/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */
#include <string>
#include <vector>
#include <json/json.h>
#include <unordered_map>

#ifndef EDDIE_DFS_H
#define EDDIE_DFS_H

typedef struct
{
    std::string nameNode;
    std::string parent;
    std::vector<std::string> childrens;
    std::vector<std::string> pseudoChildrens;
    bool visited;
} NodeDFS;


std::unordered_map<std::string, std::vector<std::string>> obtainConstraintGraph(Json::Value constraints,std::unordered_map<std::string, std::vector<int>> nodeAndConstraints);

std::vector<NodeDFS> obtainDFS(std::unordered_map<std::string, std::vector<std::string>> constraintGraph);

#endif //EDDIE_ENGINE_H