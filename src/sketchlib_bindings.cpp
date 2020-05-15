/*
 * sketchlib_bindings.cpp
 * Python bindings for pp-sketchlib
 *
 */

// pybind11 headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>

namespace py = pybind11;

#include <highfive/H5Exception.hpp>

#include "api.hpp"

/*
 * Gives the same functions as mash.py 
 * Create reference - check for existing DB (move existing code), creates sketches if none, runs query_db
 * Query db - creates query sketches, loads ref sketches, runs query_db
 */

NumpyMatrix longToSquare(const Eigen::Ref<Eigen::VectorXf>& distVec,
                          const unsigned int num_threads) {
    Eigen::VectorXf dummy_query_ref;
    Eigen::VectorXf dummy_query_query;
    NumpyMatrix converted = long_to_square(distVec, 
                                            dummy_query_ref, 
                                            dummy_query_query,
                                            num_threads);
    return(converted);
}

NumpyMatrix longToSquareMulti(const Eigen::Ref<Eigen::VectorXf>& distVec,
                            const Eigen::Ref<Eigen::VectorXf>& query_ref_distVec,
                            const Eigen::Ref<Eigen::VectorXf>& query_query_distVec,
                            const unsigned int num_threads) {
    NumpyMatrix converted = long_to_square(distVec, 
                                            query_ref_distVec, 
                                            query_query_distVec,
                                            num_threads);
    return(converted);
}

// Calls function, but returns void
void constructDatabase(const std::string& db_name,
                       const std::vector<std::string>& sample_names,
                       const std::vector<std::vector<std::string>>& file_names,
                       std::vector<size_t> kmer_lengths,
                       const size_t sketch_size,
                       const bool use_rc = true,
                       size_t min_count = 0,
                       const bool exact = false,
                       const size_t num_threads = 1)
{
    std::vector<Reference> ref_sketches = create_sketches(db_name,
                                                            sample_names, 
                                                            file_names, 
                                                            kmer_lengths,
                                                            sketch_size,
                                                            use_rc,
                                                            min_count,
                                                            exact,
                                                            num_threads);
}

NumpyMatrix queryDatabase(const std::string& ref_db_name,
                         const std::string& query_db_name,
                         const std::vector<std::string>& ref_names,
                         const std::vector<std::string>& query_names,
                         std::vector<size_t> kmer_lengths,
                         const bool jaccard = false,
                         const size_t num_threads = 1,
                         const bool use_gpu = false,
                         const int device_id = 0)
{
    if (jaccard && use_gpu) {
        throw std::runtime_error("Extracting Jaccard distances not supported on GPU");
    }
    if (!same_db_version(ref_db_name, query_db_name)) {
        std::cerr << "WARNING: versions of input databases sketches are different," \
                     " results may not be compatible" << std::endl;
    }
    
    std::vector<Reference> ref_sketches = load_sketches(ref_db_name, ref_names, kmer_lengths, false);
    std::vector<Reference> query_sketches = load_sketches(query_db_name, query_names, kmer_lengths, false);

    NumpyMatrix dists; 
#ifdef GPU_AVAILABLE
    if (use_gpu)
    {
        dists = query_db_gpu(ref_sketches,
	                        query_sketches,
                            kmer_lengths,
                            device_id);
    }
    else
    {
        dists = query_db(ref_sketches,
                query_sketches,
                kmer_lengths,
                jaccard,
                num_threads);
    }
#else
    dists = query_db(ref_sketches,
                    query_sketches,
                    kmer_lengths,
                    jaccard,
                    num_threads);
#endif
    return(dists);
}

sparse_coo sparseQuery(const std::string& ref_db_name,
                         const std::string& query_db_name,
                         const std::vector<std::string>& ref_names,
                         const std::vector<std::string>& query_names,
                         std::vector<size_t> kmer_lengths,
                         const float dist_cutoff = 0,
                         const unsigned long int kNN = 0,
                         const bool core = true,
                         const size_t num_threads = 1,
                         const bool use_gpu = false,
                         const int device_id = 0)
{
    if (!same_db_version(ref_db_name, query_db_name)) {
        std::cerr << "WARNING: versions of input databases sketches are different," \
                     " results may not be compatible" << std::endl;
    }
    
    std::vector<Reference> ref_sketches = load_sketches(ref_db_name, ref_names, kmer_lengths, false);
    std::vector<Reference> query_sketches = load_sketches(query_db_name, query_names, kmer_lengths, false);

    NumpyMatrix dists; 
#ifdef GPU_AVAILABLE
    if (use_gpu)
    {
        dists = query_db_gpu(ref_sketches,
	                        query_sketches,
                            kmer_lengths,
                            device_id);
    }
    else
    {
        dists = query_db(ref_sketches,
                query_sketches,
                kmer_lengths,
                false,
                num_threads);
    }
#else
    dists = query_db(ref_sketches,
                    query_sketches,
                    kmer_lengths,
                    false,
                    num_threads);
#endif

    unsigned int dist_col = 0;
    if (!core) {
        dist_col = 1;
    }
    NumpyMatrix long_form = longToSquare(dists.col(dist_col), num_threads);
    sparse_coo sparse_return = sparsify_dists(long_form, dist_cutoff, kNN, num_threads); 

    return(sparse_return);
}

NumpyMatrix constructAndQuery(const std::string& db_name,
                             const std::vector<std::string>& sample_names,
                             const std::vector<std::vector<std::string>>& file_names,
                             std::vector<size_t> kmer_lengths,
                             const size_t sketch_size,
                             const bool use_rc = true,
                             size_t min_count = 0,
                             const bool exact = false,
                             const bool jaccard = false,
                             const size_t num_threads = 1,
                             const bool use_gpu = false,
                             const int device_id = 0)
{
    std::vector<Reference> ref_sketches = create_sketches(db_name,
                                                            sample_names, 
                                                            file_names, 
                                                            kmer_lengths,
                                                            sketch_size,
                                                            use_rc,
                                                            min_count,
                                                            exact,
                                                            num_threads);
    NumpyMatrix dists; 
#ifdef GPU_AVAILABLE
    if (use_gpu)
    {
        dists = query_db_gpu(ref_sketches,
                             ref_sketches,
                             kmer_lengths,
                             device_id);
    }
    else
    {
        dists = query_db(ref_sketches,
                ref_sketches,
                kmer_lengths,
                jaccard,
                num_threads);
    }
#else
    dists = query_db(ref_sketches,
                     ref_sketches,
                     kmer_lengths,
                     jaccard,
                     num_threads);
#endif
    return(dists);
}

double jaccardDist(const std::string& db_name,
                   const std::string& sample1,
                   const std::string& sample2,
                   const size_t kmer_size)
{
    auto sketch_vec = load_sketches(db_name, {sample1, sample2}, {kmer_size}, false);
    return(sketch_vec.at(0).jaccard_dist(sketch_vec.at(1), kmer_size));
}

// Wrapper which makes a ref to the python/numpy array
sparse_coo sparsifyDists(const Eigen::Ref<NumpyMatrix>& denseDists,
                         const float distCutoff,
                         const unsigned long int kNN,
                         const unsigned int num_threads) {
    sparse_coo coo_idx = sparsify_dists(denseDists, distCutoff, kNN, num_threads);
}

// Wrapper which makes a ref to the python/numpy array
Eigen::VectorXf assignThreshold(const Eigen::Ref<NumpyMatrix>& distMat,
                                const int slope,
                                const double x_max,
                                const double y_max,
                                const unsigned int num_threads = 1) {
    Eigen::VectorXf assigned = assign_threshold(distMat, slope, x_max, y_max, num_threads);
    return(assigned);
}

PYBIND11_MODULE(pp_sketchlib, m)
{
  m.doc() = "Sketch implementation for PopPUNK";

  // Exported functions
  m.def("constructDatabase", &constructDatabase, "Create and save sketches", 
        py::arg("db_name"),
        py::arg("samples"),
        py::arg("files"),
        py::arg("klist"),
        py::arg("sketch_size"),
        py::arg("use_rc") = true,
        py::arg("min_count") = 0,
        py::arg("exact") = false,
        py::arg("num_threads") = 1);
  
  m.def("queryDatabase", &queryDatabase, py::return_value_policy::reference_internal, "Find distances between sketches; return all distances", 
        py::arg("ref_db_name"),
        py::arg("query_db_name"),
        py::arg("rList"),
        py::arg("qList"),
        py::arg("klist"),
        py::arg("jaccard") = false,
        py::arg("num_threads") = 1,
        py::arg("use_gpu") = false,
        py::arg("device_id") = 0);

  m.def("queryDatabaseSparse", &sparseQuery, py::return_value_policy::reference_internal, "Find distances between sketches; return a sparse matrix", 
        py::arg("ref_db_name"),
        py::arg("query_db_name"),
        py::arg("rList"),
        py::arg("qList"),
        py::arg("klist"),
        py::arg("dist_cutoff") = 0,
        py::arg("kNN") = 0,
        py::arg("core") = true,
        py::arg("num_threads") = 1,
        py::arg("use_gpu") = false,
        py::arg("device_id") = 0);

  m.def("constructAndQuery", &constructAndQuery, py::return_value_policy::reference_internal, "Create and save sketches, and get pairwise distances", 
        py::arg("db_name"),
        py::arg("samples"),
        py::arg("files"),
        py::arg("klist"),
        py::arg("sketch_size"),
        py::arg("use_rc") = true,
        py::arg("min_count") = 0,
        py::arg("exact") = false,
        py::arg("jaccard") = false,
        py::arg("num_threads") = 1,
        py::arg("use_gpu") = false,
        py::arg("device_id") = 0);

  m.def("jaccardDist", &jaccardDist, "Calculate a raw Jaccard distance",
        py::arg("db_name"),
        py::arg("ref_name"),
        py::arg("query_name"),
        py::arg("kmer_length"));

  m.def("longToSquare", &longToSquare, py::return_value_policy::reference_internal, "Convert dense long form distance matrices to square form",
        py::arg("distVec").noconvert(),
        py::arg("num_threads") = 1);

  m.def("longToSquareMulti", &longToSquareMulti, py::return_value_policy::reference_internal, "Convert dense long form distance matrices to square form (in three blocks)",
        py::arg("distVec").noconvert(),
        py::arg("query_ref_distVec").noconvert(),
        py::arg("query_query_distVec").noconvert(),
        py::arg("num_threads") = 1);

  m.def("sparsifyDists", &sparsifyDists, py::return_value_policy::reference_internal, "Transform all distances into a sparse matrix", 
        py::arg("distMat").noconvert(),
        py::arg("distCutoff") = 0,
        py::arg("kNN") = 0,
        py::arg("num_threads") = 1);

  m.def("assignThreshold", &assignThreshold, py::return_value_policy::reference_internal, "Assign samples based on their relation to a 2D boundary", 
        py::arg("distMat").noconvert(),
        py::arg("slope"),
        py::arg("x_max"),
        py::arg("y_max"),
        py::arg("num_threads") = 1);

    // Exceptions
    py::register_exception<HighFive::Exception>(m, "HDF5Exception");
}