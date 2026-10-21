#pragma once
#include <string>
#include "glad/glad.hpp"
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader {
public:
    unsigned int ID;
    inline static unsigned int activeProgram = 0;

    Shader(const std::string& vertexPath, const std::string& fragmentPath) {
        std::string vertexCode, fragmentCode;
        std::ifstream vShaderFile(vertexPath), fShaderFile(fragmentPath);
        if (!vShaderFile.is_open() || !fShaderFile.is_open()) {
            std::cerr << "Failed to open shader files\n";
            exit(EXIT_FAILURE);
        }
        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, nullptr);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() const {
        if (activeProgram == ID) return;
        glUseProgram(ID);
        activeProgram = ID;
    }
    static void stop() {
        if (activeProgram == 0) return;
        glUseProgram(0);
        activeProgram = 0;
    }

    void setMat4(const std::string &name, const float* mat) const {
        glUniformMatrix4fv(location(name), 1, GL_FALSE, mat);
    }

    void setMat4Array(const std::string& name, const float* matrices, const int count) const {
        glUniformMatrix4fv(location(name), count, GL_FALSE, matrices);
    }

    void setVec4(const std::string& name, const glm::vec4 vec) const {
        glUniform4fv(location(name), 1, &vec[0]);
    }

    void setVec2(const std::string& name, const float x, const float y) const {
        glUniform2f(location(name), x, y);
    }

    void setVec2Array(const std::string& name, const float* values, const int count) const {
        glUniform2fv(location(name), count, values);
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(location(name), 1, &value[0]);
    }

    void setFloat(const std::string& name, const float value) const {
        glUniform1f(location(name), value);
    }

    void setInt(const char * str, const int i) const {
        glUniform1i(location(str), i);
    }

    void setBool(const char* name, const bool value) const {
        glUniform1i(location(name), value ? GL_TRUE : GL_FALSE);
    }

private:
    mutable std::unordered_map<std::string, GLint> uniformLocations;

    [[nodiscard]] GLint location(const std::string& name) const {
        if (const auto found = uniformLocations.find(name); found != uniformLocations.end()) return found->second;
        const GLint value = glGetUniformLocation(ID, name.c_str());
        uniformLocations.emplace(name, value);
        return value;
    }

    static void checkCompileErrors(const unsigned int shader, const std::string& type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "Shader compilation error (" << type << "): " << infoLog << "\n";
                exit(EXIT_FAILURE);
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "Shader linking error: " << infoLog << "\n";
                exit(EXIT_FAILURE);
            }
        }
    }
};
