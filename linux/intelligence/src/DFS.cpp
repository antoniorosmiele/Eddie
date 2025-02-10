
#include "DFS.h"
#include <algorithm>
#include <iostream>

void printVectorInt(std::vector<int> vec)
{
    for(int i: vec)
        std::cout << i << ", ";
}

void printNodeAndConstraints(std::unordered_map<std::string, std::vector<int>> nodeAndConstraints)
{
    for(auto it = nodeAndConstraints.cbegin(); it != nodeAndConstraints.cend(); ++it)
    {
        std::cout << it->first << ": [";
        printVectorInt(it->second);
        std::cout << "]\n";
    }    
}


std::unordered_map<std::string, std::vector<std::string>> obtainConstraintGraph(Json::Value constraints,std::unordered_map<std::string, std::vector<int>> nodeAndConstraints)
{
    std::unordered_map<std::string, std::vector<std::string>> results;

    //printNodeAndConstraints(nodeAndConstraints);

    for (std::unordered_map<std::string, std::vector<int>>::iterator iter = nodeAndConstraints.begin(); iter != nodeAndConstraints.end(); iter++)
    {
        for (std::unordered_map<std::string, std::vector<int>>::iterator y = nodeAndConstraints.begin(); y != nodeAndConstraints.end(); y++)
        {
            std::vector<int> constrIter = iter->second;
            bool isNeighboor = false;

            if (y->first != iter->first)
            {
                for (size_t i = 0; i < constrIter.size(); i++)
                {
                    if (std::find(y->second.begin(), y->second.end(), constrIter[i]) != y->second.end())
                        isNeighboor = true;
                }

                if (results.find(iter->first) == results.end())
                {
                    std::vector<std::string> neighs;
                    neighs.push_back(y->first);
                    results.insert(std::make_pair(iter->first, neighs));
                }
                else
                {
                    (results.find(iter->first))->second.push_back(y->first);
                }
                    
            } 
        }
    }
    
    return results;

}

bool checkNeighborsNotVisited(std::vector<NodeDFS> *tree,std::vector<std::string> currentNeigh)
{
    bool result = false;

    for (auto i : currentNeigh)
    {
         if(!((fromNameToNodeDFS(i,tree))->visited))
            result = true;
    }
    
    return result;
}

NodeDFS * fromNameToNodeDFS(std::string name,std::vector<NodeDFS> *tree)
{
    if (name == "")
        return NULL;
    
    for (auto &i : *tree)
    {
        std::string nameNode = i.nameNode;
         if(nameNode == name)
            return &i;
    }

    return NULL;
}

NodeDFS * takeANeighNotVisited(std::vector<NodeDFS> *tree,std::vector<std::string> currentNeigh)
{
    for (auto i : currentNeigh)
    {
         if(!((fromNameToNodeDFS(i,tree))->visited))
            return (fromNameToNodeDFS(i,tree));
    }

    return NULL;   
}

std::vector<NodeDFS*> takeAllNeighVisited(std::vector<NodeDFS> *tree,std::vector<std::string> currentNeigh)
{
    std::vector<NodeDFS*> results;

    for (auto i : currentNeigh)
    {
         if(((fromNameToNodeDFS(i,tree))->visited))
            results.push_back(((fromNameToNodeDFS(i,tree))));        
    }

    return results;
}

std::vector<NodeDFS> obtainDFS(std::unordered_map<std::string, std::vector<std::string>> constraintGraph)
{
    std::vector<NodeDFS> results;

    for (std::unordered_map<std::string, std::vector<std::string>>::iterator i = constraintGraph.begin(); i != constraintGraph.end(); i++)
    {
        NodeDFS newNodeDFS;
        newNodeDFS.nameNode = i->first;
        newNodeDFS.parent = "";
        newNodeDFS.visited = false;
        
        results.push_back(newNodeDFS);
    }
    
    NodeDFS *currentN = &(results[0]);
    std::vector<std::string> currentNeigh = constraintGraph.find(results[0].nameNode)->second;
    NodeDFS* currentParent = NULL;
    currentN->parent = "";

    while (currentN != NULL)
    {

        //When in a new node n of G, add it to the pseudo-tree
        if(!currentN->visited)
        {
            currentN->visited = true;

            if (currentParent != NULL)
            {
                currentN->parent = currentParent->nameNode;
                currentParent->childrens.push_back(currentN->nameNode);
            }
        }

        //If n has neighbors not yet visited
        if (checkNeighborsNotVisited(&results,currentNeigh))
        {
            //then visit one unvisited neighbor of n
            currentParent = currentN;

            currentN = takeANeighNotVisited(&results,currentNeigh);
            currentNeigh = constraintGraph.find(currentN->nameNode)->second;
        }
        else
        {
            //else add pseudo-arcs for already visited neighbors

            for (auto i : takeAllNeighVisited(&results,currentNeigh))
            {
                //Lo pseudo arc va aggiunto se il vicino non è il padre o uno dei figli (to do) 
                if (std::find(currentN->childrens.begin(), currentN->childrens.end(), i->nameNode) == currentN->childrens.end() 
                && currentN->parent != i->nameNode 
                &&  std::find(currentN->pseudoParents.begin(), currentN->pseudoParents.end(), i->nameNode) == currentN->pseudoParents.end()
                &&  std::find(currentN->pseudoChildrens.begin(), currentN->pseudoChildrens.end(), i->nameNode) == currentN->pseudoChildrens.end())
                {
                    currentN->pseudoParents.push_back(i->nameNode);
                    i->pseudoChildrens.push_back(currentN->nameNode);
                }     
            }

            //backtrack to the parent of n
            currentN = currentParent;
            if (currentN != NULL)
            {
                currentNeigh = constraintGraph.find(currentN->nameNode)->second;
                currentParent = fromNameToNodeDFS(currentN->parent,&results);
            }
        }


    }

    return results;
    
}