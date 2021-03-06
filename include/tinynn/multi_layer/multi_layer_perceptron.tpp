#include "multi_layer_perceptron.hpp"

#include <iostream>
#include <blaze/math/Row.h>

namespace tnn
{
    template<typename T, size_t InSize>
    multi_layer_perceptron<T, InSize>::
    multi_layer_perceptron(
        std::function<double(double)> const& activation_func,
        std::vector<size_t> const& layer_setting,
        std::vector<vector_dyn<T>> const& bias)
        : _layers(set_layers(layer_setting)),
          _bias(),
          _activation_fun(activation_func)
    {
        auto reg_distr = std::uniform_real_distribution<double>(0, 1);
        auto seed_gen = std::random_device();
        auto rand_gen = std::mt19937(seed_gen());

        _bias.reserve(bias.size());
        // auto bias_row = blaze::Row<matrix_dyn<T>>();
        for(auto const& i : bias)
        {
            _bias.emplace_back(matrix_dyn<T>(1, i.size()));
            row(_bias.back(), 0) = blaze::trans(i);
        }

        for(auto& i : _layers)
        {
            i = map(i, [&rand_gen, &reg_distr](T elem)
                    {
                        (void)elem;
                        return reg_distr(rand_gen);
                    });
        }
    }

    template<typename T, size_t InSize>
    void
    multi_layer_perceptron<T, InSize>::
    update_weight(size_t layer_num,
                  matrix_dyn<T> const& correction)
    {
        // std::cout << "layer: " << std::endl << _layers[layer_num] <<std::endl;

        // std::cout << "correction" << std::endl << correction <<std::endl;
        _layers[layer_num] += correction;
        // std::cout << "corrected" << std::endl << _layers[layer_num]<<std::endl;
    }

    template<typename T, size_t InSize>
    std::vector<matrix_dyn<T>>
    multi_layer_perceptron<T, InSize>::
    set_layers(std::vector<size_t> const& layer_setting)
    {
        std::vector<matrix_dyn<T>> layers;
        layers.reserve(layer_setting.size());

        size_t previous_size = InSize;
        for(size_t i : layer_setting)
        {
            layers.push_back(matrix_dyn<T>(previous_size, i));
            previous_size = i;
        }

        return layers;
    }

    template<typename T, size_t InSize>
    vector_dyn<T>
    multi_layer_perceptron<T, InSize>::
    feed_layer(size_t layer_num,
               vector_dyn<T> const& input) const
    {
        auto result = blaze::evaluate(blaze::trans(input) * _layers[layer_num]);
        result = result + blaze::row(_bias[layer_num], 0);
        return blaze::trans(result);
    }

    template<typename T, size_t InSize>
    vector_dyn<T>
    multi_layer_perceptron<T, InSize>::
    eval_weight_delta(size_t layer_num,
                      vector_dyn<T> const& delta_o) const
    {
        auto result = _layers[layer_num] * delta_o;
        return result;
    }

    // template<typename T, size_t InSize>
    // matrix_dyn<T>
    // multi_layer_perceptron<T, InSize>::
    // fast_back_propagation(size_t layer_num,
    //                       matrix_dyn<T> const& delta) const 
    // {
    //     return delta * blaze::trans(_layers[layer_num]);
    // }

    template<typename T, size_t InSize>
    vector_dyn<T>
    multi_layer_perceptron<T, InSize>::
    operator()(vector<T, InSize> const& x) const
    {
        auto f = matrix_dyn<T>(1, InSize);

        for(auto i = 0u; i < InSize; ++i)
            f(0, i) = x[i];

        for(auto it = 0u; it < _layers.size(); ++it)
        {
            f *= _layers[it];
            f += _bias[it];
            f = blaze::map(f, _activation_fun); 
        }

        auto result = vector_dyn<T, blaze::rowVector>();
        result = column(f, 0);
        return result;
    }

    template<typename T, size_t InSize>
    matrix_dyn<T>
    multi_layer_perceptron<T, InSize>::
    operator()(matrix_dyn<T> const& x) const
    {
        auto f = matrix_dyn<T>(x);
        for(auto it = 0u; it < _layers.size(); ++it)
        {
            f *= _layers[it];
            f += _bias[it];
            f = blaze::map(f, _activation_fun); 
        }
        return f;
    }
}
