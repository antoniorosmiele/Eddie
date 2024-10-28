
#include "DFS.h"
#include <algorithm>

std::unordered_map<std::string, std::vector<std::string>> obtainConstraintGraph(Json::Value constraints,std::unordered_map<std::string, std::vector<int>> nodeAndConstraints)
{
    std::unordered_map<std::string, std::vector<std::string>> results;

    for (std::unordered_map<std::string, std::vector<int>>::iterator iter = nodeAndConstraints.begin(); iter != nodeAndConstraints.end(); iter++)
    {
        for (std::unordered_map<std::string, std::vector<int>>::iterator y = nodeAndConstraints.begin(); y != nodeAndConstraints.end(); iter++)
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
         if(!((std::find(tree->begin(), tree->end(), i))->visited))
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
         if(i.nameNode == name)
            return &i;
    }
}

NodeDFS * takeANeighNotVisited(std::vector<NodeDFS> *tree,std::vector<std::string> currentNeigh)
{
    for (auto i : currentNeigh)
    {
         if(!((std::find(tree->begin(), tree->end(), i))->visited))
            return &(*(std::find(tree->begin(), tree->end(), i)));
    }
        
}

std::vector<NodeDFS*> takeAllNeighVisited(std::vector<NodeDFS> *tree,std::vector<std::string> currentNeigh)
{
    std::vector<NodeDFS*> results;

    for (auto i : currentNeigh)
    {
         if((std::find(tree->begin(), tree->end(), i))->visited)
            results.push_back(&(*(std::find(tree->begin(), tree->end(), i))));        
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
        currentN->visited = true;

        if (currentParent != NULL)
        {
            currentN->parent = currentParent->nameNode;
            currentParent->childrens.push_back(currentN->nameNode);
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
                if (std::find(currentN->pseudoChildrens.begin(), currentN->pseudoChildrens.end(), i->nameNode) != currentN->pseudoChildrens.end())
                {
                    currentN->pseudoChildrens.push_back(i->nameNode);
                    i->pseudoParents.push_back(currentN->nameNode);
                }     
            }

            //backtrack to the parent of n
            currentN = currentParent;
            currentNeigh = constraintGraph.find(currentN->nameNode)->second;
            currentParent = fromNameToNodeDFS(currentN->parent,&results);
            
        }


    }
    
}