#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <cstdlib>
#include <math.h>
#include <string>
#include <sstream>
#include <lcm_iterative.h>
#include "data.h"
#include "dataContinuous.h"
#include "dataBinary.h"
#include "dataBinaryPython.h"
#include "lcm_pruned.h"
#include "query_totalfreq.h"
#include "query_weighted.h"
#include "experror.h"
#include "dataManager.h"
#include "dl85.h"
#include <chrono>

//using namespace std;
using namespace std::chrono;

bool verbose = false;


string search(Supports supports,
              Transaction ntransactions,
              Attribute nattributes,
              Class nclasses,
              Bool *data,
              Class *target,
              float maxError,
              bool stopAfterError,
              bool iterative,
              function<vector<float>(RCover *)> tids_error_class_callback,
              function<vector<float>(RCover *)> supports_error_class_callback,
              function<float(RCover *)> tids_error_callback,
              function<vector<float>(string)> example_weight_callback,
              function<float(string)> predict_error_callback,
              bool tids_error_class_is_null,
              bool supports_error_class_is_null,
              bool tids_error_is_null,
              bool example_weight_is_null,
              bool predict_error_is_null,
              int maxdepth,
              int minsup,
              int max_estimators,
              bool infoGain,
              bool infoAsc,
              bool repeatSort,
              int timeLimit,
              map<int, pair<int, int>> *continuousMap,
              bool save,
              bool verbose_param) {

//    clock_t t = clock(); //start the timer
    auto start = high_resolution_clock::now();

    //as cython can't set null to function, we use a flag to set the apropriated functions to null in c++
    function<vector<float>(RCover *)> *tids_error_class_callback_pointer = &tids_error_class_callback;
    if (tids_error_class_is_null) tids_error_class_callback_pointer = nullptr;

    function<vector<float>(RCover *)> *supports_error_class_callback_pointer = &supports_error_class_callback;
    if (supports_error_class_is_null) supports_error_class_callback_pointer = nullptr;

    function<float(RCover *)> *tids_error_callback_pointer = &tids_error_callback;
    if (tids_error_is_null) tids_error_callback_pointer = nullptr;

    function<vector<float>(string)> *example_weight_callback_pointer = &example_weight_callback;
    if (example_weight_is_null) example_weight_callback_pointer = nullptr;

    function<float(string)> *predict_error_callback_pointer = &predict_error_callback;
    if (predict_error_is_null) predict_error_callback_pointer = nullptr;

    //cout << "print " << fast_error_callback->pyFunction << endl;
    verbose = verbose_param;
    string out = "";

    DataManager *dataReader = new DataManager(supports, ntransactions, nattributes, nclasses, data, target);

    if (save) return 0; // this param is not supported yet

    //create error object and initialize it in the next
    ExpError *experror;
    experror = new ExpError_Zero;

    /* set the relevant query for the search. If only one estimator is needed or
    weighter function is null, use total query. otherwise, use weighted query */

    Query *query_total = nullptr;
    Query *query_weigthed = nullptr;
    Trie *trie = new Trie;

    if (maxError <= 0) { // in case there is no maximum error value set

        query_total = new Query_TotalFreq(trie,
                                          dataReader,
                                          experror,
                                          timeLimit,
                                          continuousMap,
                                          tids_error_class_callback_pointer,
                                          supports_error_class_callback_pointer,
                                          tids_error_callback_pointer);

        if (is_boosting)
            query_weigthed = new Query_Weighted(trie,
                                                dataReader,
                                                experror,
                                                timeLimit,
                                                continuousMap,
                                                tids_error_class_callback_pointer,
                                                supports_error_class_callback_pointer,
                                                tids_error_callback_pointer,
                                                example_weight_callback_pointer,
                                                predict_error_callback_pointer);
    } else { // when a maximum error value that must not be reached is set
        query_total = new Query_TotalFreq(trie,
                                          dataReader,
                                          experror,
                                          timeLimit,
                                          continuousMap,
                                          tids_error_class_callback_pointer,
                                          supports_error_class_callback_pointer,
                                          tids_error_callback_pointer,
                                          maxError,
                                          stopAfterError);

        if (is_boosting)
            query_weigthed = new Query_Weighted(trie,
                                                dataReader,
                                                experror,
                                                timeLimit,
                                                continuousMap,
                                                tids_error_class_callback_pointer,
                                                supports_error_class_callback_pointer,
                                                tids_error_callback_pointer,
                                                example_weight_callback_pointer,
                                                predict_error_callback_pointer,
                                                maxError,
                                                stopAfterError);
    }

    query_total->maxdepth = maxdepth;
    query_total->minsup = minsup;

    if (is_boosting) {
        query_weigthed->maxdepth = maxdepth;
        query_weigthed->minsup = minsup;
    }

    out = "TrainingDistribution: ";
    forEachClass(i) out += std::to_string(dataReader->getSupports()[i]) + " ";
    out += "\n";
//    cout << "first part c++" << endl;
//    cout << out << endl;
    out = "(nItems, nTransactions) : ( " + std::to_string(dataReader->getNAttributes() * 2) + ", " +
          std::to_string(dataReader->getNTransactions()) + " )\n";
    vector<Tree *> trees;
    float old_error_percentage = -1;

    void *lcm;
    if (iterative) {
        lcm = new LcmIterative(dataReader, query_total, trie, infoGain, infoAsc, repeatSort);
        auto start_tree = high_resolution_clock::now();
        ((LcmIterative *) lcm)->run();
        auto stop_tree = high_resolution_clock::now();
        Tree *tree_out = new Tree();
        query_total->printResult(tree_out);
        tree_out->latSize = ((LcmIterative *) lcm)->latticesize;
        tree_out->searchRt = duration_cast<milliseconds>(stop_tree - start_tree).count() / 1000.0;
        out += tree_out->to_str();
        trees.push_back(tree_out);
    } else {
        // the first run is always a total query so use it to speed up the error computation
        lcm = new LcmPruned(dataReader, query_total, trie, infoGain, infoAsc, repeatSort);
        auto start_tree = high_resolution_clock::now();
        ((LcmPruned *) lcm)->run();
        auto stop_tree = high_resolution_clock::now();
        Tree *tree_out = new Tree();
        query_total->printResult(tree_out);
        tree_out->latSize = ((LcmPruned *) lcm)->latticesize;
        tree_out->searchRt = duration_cast<milliseconds>(stop_tree - start_tree).count() / 1000.0;
        out += tree_out->to_str();
        trees.push_back(tree_out);
        float accuracy = tree_out->trainingError / dataReader->getNTransactions();
        old_error_percentage = -FLT_MAX;

        if (is_boosting) { //use the weighted query for the next of searches because we are running boosting code
            ((LcmPruned *) lcm)->query = query_weigthed;
            ((LcmPruned *) lcm)->query->boosting = true;
            for (int i = 1; i < max_estimators; ++i) {
                // set the delta value as big as possible to let the first iteration pass
                float delta_error_percentage = accuracy - old_error_percentage;
                if (delta_error_percentage < 5)
                    break;

                vector<float> new_weights = example_weight_callback(tree_out->expression);
                ((Query_Weighted*)query_weigthed)->weights = new_weights;
                //lcm = new LcmPruned(dataReader, query_weigthed, trie, infoGain, infoAsc, repeatSort);
                start_tree = high_resolution_clock::now();
                ((LcmPruned *) lcm)->run();
                stop_tree = high_resolution_clock::now();
                tree_out = new Tree();
                query_weigthed->printResult(tree_out);
                tree_out->latSize = ((LcmPruned *) lcm)->latticesize - trees[trees.size() - 1]->latSize;
                tree_out->searchRt = duration_cast<milliseconds>(stop_tree - start_tree).count() / 1000.0;
                out += tree_out->to_str();
                trees.push_back(tree_out);

                old_error_percentage = accuracy;
                accuracy = tree_out->trainingError / dataReader->getNTransactions();
            }
        }
    }

    /*if (!weight_is_null) {
        cout << "t" << endl;
        function < vector<float>(string)> callback = *example_weight_callback_pointer;
        cout << "t1" << endl;
        vector<float> r = callback();
        cout << "t2" << endl;
        cout << "liste obtenue : ";
        for (float e : r) cout << e  << ", ";
        cout << endl;
    }
    else {
        cout << "Fonction poids est None" << endl;
    }*/


    if (iterative) delete ((LcmIterative *) lcm);
    else delete ((LcmPruned *) lcm);
    delete trie;
    delete query_total;
    if (query_weigthed) delete query_weigthed;
    delete dataReader;
    delete experror;
    for (auto tree : trees) delete tree;

//    cout <<"depuis c++++x" << endl;
//    cout << out + tree_out->to_str() << "fini c++" << endl;

    auto stop = high_resolution_clock::now();
    cout << "Durée totale de l'algo : " << duration_cast<milliseconds>(stop - start).count() / 1000.0 << endl;

    return out;
}
