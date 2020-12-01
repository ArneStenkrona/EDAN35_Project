#include "parametric_shapes.hpp"
#include "core/Log.h"

#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bonobo::mesh_data
parametric_shapes::createQuad(float width, float height, unsigned int resw, unsigned int resh)
{
	auto const vertices_nb = (resw + 1) * (resh + 1);
	auto const indices_nb = resw * resh * 2;
	auto indices = std::vector<glm::uvec3>(indices_nb);
	auto vertices = std::vector<glm::vec3>(vertices_nb);
	auto normals = std::vector<glm::vec3>(vertices_nb);
	auto tangents = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);

	float stepx = (float)width / resw;
	float stepy = (float)height / resh;
	float half_width = (float)width / 2.0f;
	float half_height = (float)height / 2.0f;

	for (int y = 0; y < resh + 1; y++) {
		for (int x = 0; x < resw + 1; x++) {
			vertices[y * (resh + 1) + x] = glm::vec3(x * stepx - half_width, 0, half_height - y * stepy);
			tangents[y * (resh + 1) + x] = glm::vec3(-1, 0, 0);
			binormals[y * (resh + 1) + x] = glm::vec3(0, 0, 1);
			normals[y * (resh + 1) + x] = glm::vec3(0, 1, 0);//glm::cross(tangents[y * (resh + 1) + x], binormals[y * (resh + 1) + x]);
			float xstep = static_cast<float>(x) / static_cast<float>(resw);
			float ystep = static_cast<float>(y) / static_cast<float>(resh);
			texcoords[y * (resh + 1) + x] = glm::vec3(xstep, ystep, 0); // x,y
		}
	}

	unsigned int index = 0;
	for (int y = 0; y < resh; y++) {
		for (int x = 0; x < resw; x++) {
			indices[index] = glm::uvec3(x + y * (resw + 1), x + 1 + y * (resw + 1), x + (y + 1) * (resw + 1));
			index++;
			indices[index] = glm::uvec3(x + 1 + y * (resw + 1), x + 1 + (y + 1) * (resw + 1), x + (y + 1) * (resw + 1));
			index++;
		}
	}

	bonobo::mesh_data data;
	glGenVertexArrays(1, &data.vao);
	assert(data.vao != 0u);
	glBindVertexArray(data.vao);

	auto const vertices_offset = 0u;
	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
		+ normals_size
		+ texcoords_size
		+ tangents_size
		+ binormals_size
		);

	glGenBuffers(1, &data.bo);
	assert(data.bo != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));

	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	data.indices_nb = indices.size() * 3u;
	glGenBuffers(1, &data.ibo);
	assert(data.ibo != 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(indices.data()), GL_STATIC_DRAW);

	glBindVertexArray(0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}

bonobo::mesh_data
parametric_shapes::createCube(float side)
{
	// Cube
	float offset = side / 2.0f;

	auto const vertices = std::array<glm::vec3, 24> {
		// Front
		glm::vec3(-offset, -offset, offset),
			glm::vec3(offset, -offset, offset),
			glm::vec3(offset, offset, offset),
			glm::vec3(-offset, offset, offset),

			// Back
			glm::vec3(offset, -offset, -offset),
			glm::vec3(-offset, -offset, -offset),
			glm::vec3(-offset, offset, -offset),
			glm::vec3(offset, offset, -offset),

			// Left
			glm::vec3(-offset, -offset, -offset),
			glm::vec3(-offset, -offset, offset),
			glm::vec3(-offset, offset, offset),
			glm::vec3(-offset, offset, -offset),

			// Right
			glm::vec3(offset, -offset, offset),
			glm::vec3(offset, -offset, -offset),
			glm::vec3(offset, offset, -offset),
			glm::vec3(offset, offset, offset),

			// Top
			glm::vec3(-offset, offset, offset),
			glm::vec3(offset, offset, offset),
			glm::vec3(offset, offset, -offset),
			glm::vec3(-offset, offset, -offset),

			// Bottom
			glm::vec3(-offset, -offset, -offset),
			glm::vec3(offset, -offset, -offset),
			glm::vec3(offset, -offset, offset),
			glm::vec3(-offset, -offset, offset),
	};

	auto indices = std::array<glm::uvec3, 12>();
	for (int i = 0; i < 12; i += 2) {
		indices[i] = glm::uvec3(0u + 2 * i, 1u + 2 * i, 2u + 2 * i);
		indices[i + 1] = glm::uvec3(0u + 2 * i, 2u + 2 * i, 3u + 2 * i);
	}

	auto normals = std::array<glm::vec3, 24>();
	for (int i = 0; i < 4; i++) normals[i] = glm::vec3(0, 0, 1);
	for (int i = 0; i < 4; i++) normals[i + 4] = glm::vec3(0, 0, -1);
	for (int i = 0; i < 4; i++) normals[i + 8] = glm::vec3(-1, 0, 0);
	for (int i = 0; i < 4; i++) normals[i + 12] = glm::vec3(0, 0, 1);
	for (int i = 0; i < 4; i++) normals[i + 16] = glm::vec3(0, 1, 0);
	for (int i = 0; i < 4; i++) normals[i + 20] = glm::vec3(0, -1, 0);

	auto texcoords = std::array<glm::vec3, 24>();
	for (int i = 0; i < 24; i += 4) {
		texcoords[i] = glm::vec3(0, 0, 0);
		texcoords[i + 1] = glm::vec3(1, 0, 0);
		texcoords[i + 2] = glm::vec3(1, 1, 0);
		texcoords[i + 3] = glm::vec3(0, 1, 0);
	}

	bonobo::mesh_data data;
	glGenVertexArrays(1, &data.vao);
	assert(data.vao != 0u);
	glBindVertexArray(data.vao);

	auto const vertices_offset = 0u;
	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size);

	glGenBuffers(1, &data.bo);
	assert(data.bo != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	data.indices_nb = indices.size() * 3u;
	glGenBuffers(1, &data.ibo);
	assert(data.ibo != 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(indices.data()), GL_STATIC_DRAW);

	glBindVertexArray(0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}

bonobo::mesh_data
parametric_shapes::createSphere(unsigned int const res_theta,
	unsigned int const res_phi, float const radius)
{
	auto const vertices_nb = res_phi * res_theta;

	auto vertices = std::vector<glm::vec3>(vertices_nb);
	auto normals = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);
	auto tangents = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);

	float theta = 0.0f;    // 'stepping'-variable for theta: will go 0 - 2PI
	float dtheta = glm::two_pi<float>() / (static_cast<float>(res_theta) - 1.0f); // step size, depending on the resolution

	float phi = 0.0f;     // 'stepping'-variable for phi:
	float dphi = glm::pi<float>() / (static_cast<float>(res_phi) - 1.0f); // step size, depending on the resolution

	size_t index = 0u;

	for (unsigned int i = 0u; i < res_theta; i++) {
		float cos_theta = std::cos(theta), sin_theta = std::sin(theta);
		phi = 0.0f;
		for (unsigned int j = 0u; j < res_phi; j++) {
			float cos_phi = std::cos(phi), sin_phi = std::sin(phi);

			//vertex
			vertices[index] = glm::vec3(
				radius * sin_theta * sin_phi,
				-radius * cos_phi,
				radius * cos_theta * sin_phi
			);

			// texture coordinates
			texcoords[index] = glm::vec3(
				static_cast<float>(i) / (static_cast<float>(res_theta) - 1.0f),
				static_cast<float>(j) / (static_cast<float>(res_phi) - 1.0f),
				0.0f);

			// tangent
			auto t = glm::vec3(cos_theta, 0.0f, -sin_theta);
			t = glm::normalize(t);
			tangents[index] = t;

			// binormal
			auto b = glm::vec3(sin_theta * cos_phi, sin_phi, cos_theta * cos_phi);
			b = glm::normalize(b);
			binormals[index] = b;

			// normal
			//auto const n = glm::cross(t, b);
			//normals[index] = n;
			normals[index] = glm::normalize(vertices[index]);
			++index;
			phi += dphi;
		}

		theta += dtheta;
	}

	// create index array
	auto indices = std::vector<glm::uvec3>(2u * (res_theta - 1u) * (res_phi - 1u));

	// generate indices iteratively
	index = 0u;
	for (unsigned int i = 0u; i < res_theta - 1u; ++i)
	{
		for (unsigned int j = 0u; j < res_phi - 1u; ++j)
		{
			indices[index] = glm::uvec3(res_phi * i + j,
				res_phi * i + j + 1u,
				res_phi * i + j + 1u + res_phi);
			++index;

			indices[index] = glm::uvec3(res_phi * i + j,
				res_phi * i + j + res_phi + 1u,
				res_phi * i + j + res_phi);
			++index;
		}
	}

	bonobo::mesh_data data;
	glGenVertexArrays(1, &data.vao);
	assert(data.vao != 0u);
	glBindVertexArray(data.vao);

	auto const vertices_offset = 0u;
	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
		+ normals_size
		+ texcoords_size
		+ tangents_size
		+ binormals_size
		);


	glGenBuffers(1, &data.bo);
	assert(data.bo != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));

	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));

	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));

	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	data.indices_nb = indices.size() * 3u;
	glGenBuffers(1, &data.ibo);
	assert(data.ibo != 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(indices.data()), GL_STATIC_DRAW);

	glBindVertexArray(0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

	return data;
}
