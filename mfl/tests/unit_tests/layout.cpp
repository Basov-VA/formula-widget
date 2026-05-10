#include "mfl/layout.hpp"

#include "framework/doctest.hpp"
#include "framework/mock_font_face.hpp"

namespace mfl
{
    using namespace units_literals;

    TEST_SUITE("layout")
    {
        TEST_CASE("layout call with valid input")
        {
            const auto result = layout(R"(\frac{1}{x+y})", 10_pt, create_mock_font_face);
            CHECK(!result.error);
            CHECK(result.width > 24_pt);
            CHECK(result.height > 11_pt);
            CHECK(result.glyphs.size() == 4);
            CHECK(result.lines.size() == 1);
        }

        TEST_CASE("layout call with invalid input")
        {
            const auto result = layout(R"(\frac{1{x})", 10_pt, create_mock_font_face);
            CHECK(result.error);
            CHECK(result.width == 0_pt);
            CHECK(result.height == 0_pt);
            CHECK(result.glyphs.empty());
            CHECK(result.lines.empty());
        }

        TEST_CASE("layout tree structure for simple fraction")
        {
            const auto result = layout(R"(\frac{a}{b})", 10_pt, create_mock_font_face);
            CHECK(!result.error);

            // Check that we have a tree
            CHECK(result.tree.type == formula_node_type::root);

            // Check that the root has children
            CHECK(!result.tree.children.empty());

            // Check that we have a fraction node
            bool found_fraction = false;
            for (const auto& child : result.tree.children)
            {
                if (child.type == formula_node_type::fraction)
                {
                    found_fraction = true;
                    // Check that fraction has children (numerator and denominator)
                    CHECK(!child.children.empty());

                    // Check for numerator and denominator
                    bool found_numerator = false;
                    bool found_denominator = false;
                    for (const auto& fraction_child : child.children)
                    {
                        if (fraction_child.type == formula_node_type::numerator)
                        {
                            found_numerator = true;
                            // Numerator should have a symbol child
                            CHECK(!fraction_child.children.empty());
                        }
                        else if (fraction_child.type == formula_node_type::denominator)
                        {
                            found_denominator = true;
                            // Denominator should have a symbol child
                            CHECK(!fraction_child.children.empty());
                        }
                    }
                    CHECK(found_numerator);
                    CHECK(found_denominator);
                }
            }
            CHECK(found_fraction);
        }

        TEST_CASE("layout tree structure for simple script")
        {
            const auto result = layout(R"(a_1)", 10_pt, create_mock_font_face);
            CHECK(!result.error);

            // Check that we have a tree
            CHECK(result.tree.type == formula_node_type::root);

            // Check that the root has children
            CHECK(!result.tree.children.empty());

            // Check that we have a script nucleus and subscript
            bool found_nucleus = false;
            bool found_subscript = false;
            for (const auto& child : result.tree.children)
            {
                if (child.type == formula_node_type::script_nucleus)
                {
                    found_nucleus = true;
                    // Nucleus should have a symbol child
                    CHECK(!child.glyph_indices.empty());
                }
                else if (child.type == formula_node_type::subscript)
                {
                    found_subscript = true;
                    // Subscript should have a symbol child
                    CHECK(!child.glyph_indices.empty());
                }
            }
            CHECK(found_nucleus);
            CHECK(found_subscript);
        }

        TEST_CASE("layout tree structure for simple radical")
        {
            const auto result = layout(R"(\sqrt{a})", 10_pt, create_mock_font_face);
            CHECK(!result.error);

            // Check that we have a tree
            CHECK(result.tree.type == formula_node_type::root);

            // Check that the root has children
            CHECK(!result.tree.children.empty());

            // Check that we have a radical and radicand
            bool found_radical = false;
            bool found_radicand = false;
            for (const auto& child : result.tree.children)
            {
                if (child.type == formula_node_type::radical)
                {
                    found_radical = true;
                }
                else if (child.type == formula_node_type::radicand)
                {
                    found_radicand = true;
                    // Radicand should have a symbol child
                    CHECK(!child.glyph_indices.empty());
                }
            }
            CHECK(found_radical);
            CHECK(found_radicand);
        }

                TEST_CASE("layout bounding box computation for simple fraction")
                {
                    const auto result = layout(R"(\frac{a}{b})", 10_pt, create_mock_font_face);
                    CHECK(!result.error);

                    // Check that we have a tree
                    CHECK(result.tree.type == formula_node_type::root);

                    // Root bounding box should encompass the entire formula
                    CHECK(result.tree.bbox_width > 0_pt);
                    CHECK(result.tree.bbox_height > 0_pt);

                    // Check that the root has children
                    CHECK(!result.tree.children.empty());

                    // Check that we have a fraction node with valid bounding box
                    bool found_fraction = false;
                    for (const auto& child : result.tree.children)
                    {
                        if (child.type == formula_node_type::fraction)
                        {
                            found_fraction = true;
                            // Fraction bounding box should be valid
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);

                            // Check that fraction has children (numerator and denominator)
                            CHECK(!child.children.empty());

                            // Check for numerator and denominator with valid bounding boxes
                            bool found_numerator = false;
                            bool found_denominator = false;
                            for (const auto& fraction_child : child.children)
                            {
                                if (fraction_child.type == formula_node_type::numerator)
                                {
                                    found_numerator = true;
                                    CHECK(fraction_child.bbox_width > 0_pt);
                                    CHECK(fraction_child.bbox_height > 0_pt);
                                    // Numerator should have a symbol
                                    CHECK(!fraction_child.glyph_indices.empty());
                                }
                                else if (fraction_child.type == formula_node_type::denominator)
                                {
                                    found_denominator = true;
                                    CHECK(fraction_child.bbox_width > 0_pt);
                                    CHECK(fraction_child.bbox_height > 0_pt);
                                    // Denominator should have a symbol
                                    CHECK(!fraction_child.glyph_indices.empty());
                                }
                            }
                            CHECK(found_numerator);
                            CHECK(found_denominator);
                        }
                    }
                    CHECK(found_fraction);
                }

                TEST_CASE("layout bounding box computation for simple script")
                {
                    const auto result = layout(R"(a_1)", 10_pt, create_mock_font_face);
                    CHECK(!result.error);

                    // Check that we have a tree
                    CHECK(result.tree.type == formula_node_type::root);

                    // Root bounding box should encompass the entire formula
                    CHECK(result.tree.bbox_width > 0_pt);
                    CHECK(result.tree.bbox_height > 0_pt);

                    // Check that the root has children
                    CHECK(!result.tree.children.empty());

                    // Check that we have a script nucleus and subscript with valid bounding boxes
                    bool found_nucleus = false;
                    bool found_subscript = false;
                    for (const auto& child : result.tree.children)
                    {
                        if (child.type == formula_node_type::script_nucleus)
                        {
                            found_nucleus = true;
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);
                            // Nucleus should have a symbol
                            CHECK(!child.glyph_indices.empty());
                        }
                        else if (child.type == formula_node_type::subscript)
                        {
                            found_subscript = true;
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);
                            // Subscript should have a symbol
                            CHECK(!child.glyph_indices.empty());
                        }
                    }
                    CHECK(found_nucleus);
                    CHECK(found_subscript);
                }

                TEST_CASE("layout bounding box computation for simple radical")
                {
                    const auto result = layout(R"(\sqrt{a})", 10_pt, create_mock_font_face);
                    CHECK(!result.error);

                    // Check that we have a tree
                    CHECK(result.tree.type == formula_node_type::root);

                    // Root bounding box should encompass the entire formula
                    CHECK(result.tree.bbox_width > 0_pt);
                    CHECK(result.tree.bbox_height > 0_pt);

                    // Check that the root has children
                    CHECK(!result.tree.children.empty());

                    // Check that we have a radical and radicand with valid bounding boxes
                    bool found_radical = false;
                    bool found_radicand = false;
                    for (const auto& child : result.tree.children)
                    {
                        if (child.type == formula_node_type::radical)
                        {
                            found_radical = true;
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);
                        }
                        else if (child.type == formula_node_type::radicand)
                        {
                            found_radicand = true;
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);
                            // Radicand should have a symbol
                            CHECK(!child.glyph_indices.empty());
                        }
                    }
                    CHECK(found_radical);
                    CHECK(found_radicand);
                }

                TEST_CASE("layout tree structure for fraction with scripts")
                {
                    const auto result = layout(R"(\frac{a_1}{b^2})", 10_pt, create_mock_font_face);
                    CHECK(!result.error);

                    // Check that we have a tree
                    CHECK(result.tree.type == formula_node_type::root);

                    // Check that the root has children
                    CHECK(!result.tree.children.empty());

                    // Check that we have a fraction node
                    bool found_fraction = false;
                    for (const auto& child : result.tree.children)
                    {
                        if (child.type == formula_node_type::fraction)
                        {
                            found_fraction = true;
                            // Check that fraction has children (numerator and denominator)
                            CHECK(!child.children.empty());

                            // Check for numerator and denominator
                            bool found_numerator = false;
                            bool found_denominator = false;
                            for (const auto& fraction_child : child.children)
                            {
                                if (fraction_child.type == formula_node_type::numerator)
                                {
                                    found_numerator = true;
                                    // Numerator should have a script nucleus and subscript
                                    bool found_nucleus = false;
                                    bool found_subscript = false;
                                    for (const auto& num_child : fraction_child.children)
                                    {
                                        if (num_child.type == formula_node_type::script_nucleus)
                                        {
                                            found_nucleus = true;
                                        }
                                        else if (num_child.type == formula_node_type::subscript)
                                        {
                                            found_subscript = true;
                                        }
                                    }
                                    CHECK(found_nucleus);
                                    CHECK(found_subscript);
                                }
                                else if (fraction_child.type == formula_node_type::denominator)
                                {
                                    found_denominator = true;
                                    // Denominator should have a script nucleus and superscript
                                    bool found_nucleus = false;
                                    bool found_superscript = false;
                                    for (const auto& den_child : fraction_child.children)
                                    {
                                        if (den_child.type == formula_node_type::script_nucleus)
                                        {
                                            found_nucleus = true;
                                        }
                                        else if (den_child.type == formula_node_type::superscript)
                                        {
                                            found_superscript = true;
                                        }
                                    }
                                    CHECK(found_nucleus);
                                    CHECK(found_superscript);
                                }
                            }
                            CHECK(found_numerator);
                            CHECK(found_denominator);
                        }
                    }
                    CHECK(found_fraction);
                }

                TEST_CASE("layout bounding box computation for fraction with scripts")
                {
                    const auto result = layout(R"(\frac{a_1}{b^2})", 10_pt, create_mock_font_face);
                    CHECK(!result.error);

                    // Check that we have a tree
                    CHECK(result.tree.type == formula_node_type::root);

                    // Root bounding box should encompass the entire formula
                    CHECK(result.tree.bbox_width > 0_pt);
                    CHECK(result.tree.bbox_height > 0_pt);

                    // Check that the root has children
                    CHECK(!result.tree.children.empty());

                    // Check that we have a fraction node with valid bounding box
                    bool found_fraction = false;
                    for (const auto& child : result.tree.children)
                    {
                        if (child.type == formula_node_type::fraction)
                        {
                            found_fraction = true;
                            // Fraction bounding box should be valid
                            CHECK(child.bbox_width > 0_pt);
                            CHECK(child.bbox_height > 0_pt);

                            // Check that fraction has children (numerator and denominator)
                            CHECK(!child.children.empty());

                            // Check for numerator and denominator with valid bounding boxes
                            bool found_numerator = false;
                            bool found_denominator = false;
                            for (const auto& fraction_child : child.children)
                            {
                                if (fraction_child.type == formula_node_type::numerator)
                                {
                                    found_numerator = true;
                                    CHECK(fraction_child.bbox_width > 0_pt);
                                    CHECK(fraction_child.bbox_height > 0_pt);
                                    // Numerator should have symbols
                                    CHECK(!fraction_child.glyph_indices.empty());
                                }
                                else if (fraction_child.type == formula_node_type::denominator)
                                {
                                    found_denominator = true;
                                    CHECK(fraction_child.bbox_width > 0_pt);
                                    CHECK(fraction_child.bbox_height > 0_pt);
                                    // Denominator should have symbols
                                    CHECK(!fraction_child.glyph_indices.empty());
                                }
                            }
                            CHECK(found_numerator);
                            CHECK(found_denominator);
                        }
                    }
                    CHECK(found_fraction);
                }
            }
        }
