#include <algorithm>
#include <iomanip>
#include <memory>
#include <vector>
#include <map>
#include <cassert>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
using namespace std;

class Counter {
public:
    Counter() { ++ count_; }
    ~Counter() { --count_; }
    static int count() { return count_; }
    static void reset() { count_ = 0; }
private:
    static int count_;
};

int Counter::count_ = 0;

//Basic design idea:
//transfer the ownership to the graph
//this corresponds to the "ShrinkToFit" method which already indicated this
//Node only stores the backlink to the graph for adding/removing stuff
//Removing children only removes links.
//Shrink to fit will free unlinked nodes that are not reachable anymore.
//not optimized for performance.

class MyGraph {
public:
    class Node : public Counter {
        friend class MyGraph;
        MyGraph* graph;

    public:
        Node()
                : graph(nullptr)
        { }

        void AddChild(const shared_ptr<Node>& node) {
            assert(node->graph==nullptr || node->graph == graph);
            assert(graph!=nullptr);
            node->graph = graph;
            graph->AddNode(node);
            graph->AddLink(this,node.get());
        }

        //if a node is linked multiple times, only one link is removed
        void RemoveChild(const shared_ptr<Node>& node) {
            graph->RemoveLink(this,node.get());
        }

    };

    MyGraph() : root(nullptr) {}

    void SetRoot(const shared_ptr<Node>& node) {
        //otherwise we'd need to re-propagate root through all the graph
        assert(!root);
        AddNode(node);
        root = node.get();
        node->graph = this;
    }

    void ShrinkToFit() {
        //find unlinked nodes (from root)
        auto connectedNodes = GetConnectedNodes();
        for (auto iter = nodes.begin(); iter!=nodes.end(); ) {
            auto nodeptr = (*iter).get();
            if (connectedNodes.find(nodeptr)==connectedNodes.end()) {
                iter = nodes.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }

    static auto MakeNode() { return make_shared<MyGraph::Node>(); }

private:
    Node* root; //entry point of the graph
    multimap<Node*, Node*> links; //storage for directional links
    vector<shared_ptr<Node>> nodes; //owner of the nodes

    //adds a link to the storage of directional links
    //allows adding duplicates
    void AddLink(Node* from, Node* to) {
        links.insert(make_pair(from,to));
    }

    //adds a node if not already present
    void AddNode(shared_ptr<Node> node) {
        auto iter = find(nodes.begin(),nodes.end(),node);
        if (iter == nodes.end()) {
            nodes.push_back(node);
        }
    }

    //removes one link from one node to another if available
    //does not remove all links if multiple links are present
    void RemoveLink(Node* from, Node* to) {
        auto children = links.equal_range(from);
        for (auto iter = children.first; iter!=children.second; ++iter) {
            if (iter->second==to) {
                links.erase(iter);
                break; //erase only the first link
            }
        }
    }

    //returns a set of all nodes that are connected to root
    set<Node*> GetConnectedNodes() {
        set<Node*> result;
        vector<Node*> stack; //manual stack to allow deeper nesting
        stack.push_back(root); //start from root
        while (!stack.empty()) { //continue until no work to do
            auto current = stack.back();
            stack.pop_back();
            result.insert(current);
            //add children to stack if not already visisted (= in result)
            auto children = links.equal_range(current);
            for (auto iter = children.first; iter != children.second; ++iter) {
                auto linkTarget = iter->second;
                if (result.find(linkTarget)==result.end()) {
                    stack.push_back(linkTarget);
                }
            }
        }
        return result;
    }
};

TEST_CASE("removing child removes subtrees") {
    Counter::reset();
    MyGraph g;
    {
        auto a = MyGraph::MakeNode();
        g.SetRoot(a);
        auto b = MyGraph::MakeNode();
        a->AddChild(b);
        auto c = MyGraph::MakeNode();
        b->AddChild(c);
        a->RemoveChild(b);
    }
    g.ShrinkToFit();
    REQUIRE(Counter::count() == 1);
}

TEST_CASE("removing chld removes sub-graphs with cycles") {
    Counter::reset();
    MyGraph g;
    {
        auto a = MyGraph::MakeNode();
        g.SetRoot(a);
        auto b = MyGraph::MakeNode();
        a->AddChild(b);
        auto c = MyGraph::MakeNode();
        b->AddChild(c);
        auto d = MyGraph::MakeNode();
        b->AddChild(d);
        d->AddChild(b);
        a->RemoveChild(b);
    }
    g.ShrinkToFit();
    REQUIRE(Counter::count() == 1);
}

TEST_CASE("removing nothing yields the correct node-count") {
    Counter::reset();
    MyGraph g;
    {
        auto a = MyGraph::MakeNode();
        g.SetRoot(a);
        auto b = MyGraph::MakeNode();
        a->AddChild(b);
        auto c = MyGraph::MakeNode();
        b->AddChild(c);
        auto d = MyGraph::MakeNode();
        b->AddChild(d);
        d->AddChild(b);
    }
    g.ShrinkToFit();
    REQUIRE(Counter::count() == 4);
}

TEST_CASE("break circle should not remove the node") {
    Counter::reset();
    MyGraph g;
    {
        auto a = MyGraph::MakeNode();
        g.SetRoot(a);
        auto b = MyGraph::MakeNode();
        a->AddChild(b);
        auto c = MyGraph::MakeNode();
        b->AddChild(c);
        auto d = MyGraph::MakeNode();
        b->AddChild(d);
        d->AddChild(b);
        d->RemoveChild(b);
    }
    g.ShrinkToFit();
    REQUIRE(Counter::count() == 4);
}